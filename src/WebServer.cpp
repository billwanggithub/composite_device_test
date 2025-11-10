#include "WebServer.h"
#include "CommandParser.h"
#include "ArduinoJson.h"
#include <WiFi.h>

// 外部變數（從 main.cpp）
extern CommandParser parser;

WebServerManager::WebServerManager() {
    // Constructor
}

bool WebServerManager::begin(WiFiSettings* wifiSettings,
                             WiFiManager* wifiManager,
                             StatusLED* statusLED,
                             PeripheralManager* peripheralManager,
                             WiFiSettingsManager* wifiSettingsManager) {
    if (!wifiSettings || !wifiManager || !peripheralManager) {
        Serial.println("❌ WebServerManager::begin() - NULL pointer!");
        return false;
    }

    pWiFiSettings = wifiSettings;
    // Motor control is now integrated into UART1Mux (accessed via peripheralManager)
    pWiFiManager = wifiManager;
    pStatusLED = statusLED;
    pPeripheralManager = peripheralManager;
    pWiFiSettingsManager = wifiSettingsManager;

    // Create server instance
    server = new AsyncWebServer(wifiSettings->web_port);
    ws = new AsyncWebSocket("/ws");

    Serial.printf("✅ Web Server initialized on port %d\n", wifiSettings->web_port);
    return true;
}

bool WebServerManager::start() {
    if (!server || !ws) {
        Serial.println("❌ Web Server not initialized!");
        return false;
    }

    // Setup WebSocket
    setupWebSocket();
    server->addHandler(ws);

    // Setup HTTP routes
    setupRoutes();

    // Start server
    server->begin();
    running = true;

    Serial.println("✅ Web Server started");
    Serial.printf("  Access at: http://%s/\n", pWiFiManager->getIPAddress().c_str());

    return true;
}

void WebServerManager::stop() {
    if (server) {
        server->end();
        running = false;
        Serial.println("📡 Web Server stopped");
    }
}

void WebServerManager::update() {
    if (!running || !ws) {
        return;
    }

    // Clean up WebSocket clients
    ws->cleanupClients();

    // Broadcast RPM periodically
    unsigned long now = millis();
    if (now - lastWSBroadcast >= WS_BROADCAST_INTERVAL_MS) {
        lastWSBroadcast = now;

        if (ws->count() > 0) {
            broadcastStatus();
        }
    }
}

bool WebServerManager::isRunning() const {
    return running;
}

uint32_t WebServerManager::getWSClientCount() const {
    return ws ? ws->count() : 0;
}

void WebServerManager::broadcastRPM(float rpm) {
    if (!ws || ws->count() == 0) {
        return;
    }

    StaticJsonDocument<128> doc;
    doc["type"] = "rpm";
    doc["rpm"] = rpm;

    String json;
    serializeJson(doc, json);
    ws->textAll(json);
}

void WebServerManager::broadcastStatus() {
    if (!ws || ws->count() == 0 || !pPeripheralManager) {
        return;
    }

    StaticJsonDocument<512> doc;
    doc["type"] = "status";
    // Motor control now via UART1
    doc["rpm"] = pPeripheralManager->getUART1().getCalculatedRPM();
    doc["raw_freq"] = pPeripheralManager->getUART1().getRPMFrequency();  // Raw frequency instead of raw RPM
    doc["freq"] = pPeripheralManager->getUART1().getPWMFrequency();
    doc["duty"] = pPeripheralManager->getUART1().getPWMDuty();
    // Ramping and emergency stop features removed in v3.0
    doc["uptime"] = millis() / 1000;  // System uptime in seconds

    String json;
    serializeJson(doc, json);
    ws->textAll(json);
}

void WebServerManager::setupWebSocket() {
    ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->handleWebSocketEvent(server, client, type, arg, data, len);
    });
}

void WebServerManager::handleWebSocketEvent(AsyncWebSocket *server,
                                            AsyncWebSocketClient *client,
                                            AwsEventType type,
                                            void *arg,
                                            uint8_t *data,
                                            size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client #%u connected from %s\n",
                         client->id(), client->remoteIP().toString().c_str());
            // Send initial status
            broadcastStatus();
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] Client #%u disconnected\n", client->id());
            break;

        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;

        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WebServerManager::handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;

    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;  // Null terminate
        String message = (char*)data;

        Serial.printf("[WS] Received: %s\n", message.c_str());

        // 首先嘗試作為 JSON 命令解析
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);

        if (!error && doc.containsKey("cmd")) {
            // JSON 格式的命令（保留向後兼容性）
            const char* cmd = doc["cmd"];
            if (!cmd) {
                return;
            }

            // Handle legacy JSON commands (motor control now via UART1)
            if (strcmp(cmd, "set_freq") == 0) {
                uint32_t freq = doc["value"];
                if (pPeripheralManager) {
                    pPeripheralManager->getUART1().setPWMFrequency(freq);
                    broadcastStatus();
                }
            }
            else if (strcmp(cmd, "set_duty") == 0) {
                float duty = doc["value"];
                if (pPeripheralManager) {
                    pPeripheralManager->getUART1().setPWMDuty(duty);
                    broadcastStatus();
                }
            }
            else if (strcmp(cmd, "stop") == 0) {
                // Simple stop: set duty to 0% (emergency stop removed in v3.0)
                if (pPeripheralManager) {
                    pPeripheralManager->getUART1().setPWMDuty(0.0);
                    broadcastStatus();
                }
            }
            else if (strcmp(cmd, "clear_error") == 0) {
                // No-op: emergency stop feature removed in v3.0
                broadcastStatus();
            }
            else if (strcmp(cmd, "get_status") == 0) {
                broadcastStatus();
            }
        } else {
            // 作為文本命令處理（支持完整的命令解析系統）
            // 相同的命令系統用於 CDC 和 HID
            String trimmed = message;
            trimmed.trim();

            // 跳過空命令
            if (trimmed.length() == 0) {
                return;
            }

            Serial.printf("[WS] 文本命令: %s\n", trimmed.c_str());

            // 取得客戶端 ID
            uint32_t client_id = info->num;

            // 創建 WebSocket 響應對象
            WebSocketResponse wsResponse((void*)ws, client_id);

            // 使用命令解析器處理命令
            bool commandProcessed = parser.processCommand(trimmed, &wsResponse, CMD_SOURCE_WEBSOCKET);

            // 取得響應文本
            String response = wsResponse.getResponse();

            Serial.printf("[WS] 命令已處理: %s, 響應長度: %d\n",
                         commandProcessed ? "是" : "否", response.length());

            // 如果沒有響應，檢查是否是未知命令
            if (response.length() == 0) {
                if (!commandProcessed) {
                    // 命令未識別
                    response = "❌ 未知命令: " + trimmed;
                    Serial.printf("[WS] 未知命令，發送錯誤消息\n");
                } else {
                    // 命令被處理但沒有響應（不應該發生）
                    response = "✓ 命令已執行\n";
                    Serial.printf("[WS] 命令被處理但沒有響應\n");
                }
            }

            // 發送響應給客戶端
            AsyncWebSocketClient* client = ws->client(client_id);
            if (client) {
                Serial.printf("[WS] 發送響應到客戶端 %d: %d 字節\n", client_id, response.length());
                client->text(response);
            } else {
                Serial.printf("[WS] ❌ 找不到客戶端 %d\n", client_id);
            }

            // 廣播狀態更新
            if (commandProcessed) {
                broadcastStatus();
            }
        }
    }
}

void WebServerManager::setupRoutes() {
    // Serve main page from SPIFFS (root path)
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/index.html")) {
            request->send(SPIFFS, "/index.html", "text/html");
        } else {
            request->send(200, "text/html", generateIndexHTML());  // Fallback to embedded HTML
        }
    });

    // Serve main page from SPIFFS (explicit index.html path)
    server->on("/index.html", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/index.html")) {
            request->send(SPIFFS, "/index.html", "text/html");
        } else {
            request->send(200, "text/html", generateIndexHTML());  // Fallback to embedded HTML
        }
    });

    // Serve settings page from SPIFFS
    server->on("/settings.html", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/settings.html")) {
            request->send(SPIFFS, "/settings.html", "text/html");
        } else {
            request->send(404, "text/plain", "Settings page not found");
        }
    });

    // Serve peripherals page from SPIFFS
    server->on("/peripherals.html", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/peripherals.html")) {
            request->send(SPIFFS, "/peripherals.html", "text/html");
        } else {
            request->send(404, "text/plain", "Peripherals page not found");
        }
    });

    // Serve console page from SPIFFS
    server->on("/console.html", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/console.html")) {
            request->send(SPIFFS, "/console.html", "text/html");
        } else {
            request->send(404, "text/plain", "Console page not found");
        }
    });

    // REST API endpoints
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetStatus(request);
    });

    server->on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetSettings(request);
    });

    server->on("/api/motor/freq", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSetPWMFreq(request);
    });

    server->on("/api/motor/duty", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSetPWMDuty(request);
    });

    server->on("/api/motor/stop", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleMotorStop(request);
    });

    server->on("/api/motor/clear-error", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleClearError(request);
    });

    server->on("/api/settings/save", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSaveSettings(request);
    });

    server->on("/api/settings/load", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleLoadSettings(request);
    });

    server->on("/api/settings/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleResetSettings(request);
    });

    server->on("/api/wifi/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetWiFiStatus(request);
    });

    server->on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleScanNetworks(request);
    });

    // New API endpoints for clone implementation
    server->on("/api/rpm", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetRPM(request);
    });

    server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetConfig(request);
    });

    server->on("/api/config", HTTP_POST,
        [this](AsyncWebServerRequest *request) {
            handlePostConfig(request);
        },
        NULL,  // upload handler
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // Body handler - store body data for processing in handlePostConfig
            if (index == 0) {
                request->_tempObject = new String();
            }
            String* bodyStr = (String*)request->_tempObject;
            for (size_t i = 0; i < len; i++) {
                bodyStr->concat((char)data[i]);
            }
        }
    );

    server->on("/api/pwm", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostPWM(request);
    });

    server->on("/api/pole-pairs", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostPolePairs(request);
    });

    server->on("/api/max-frequency", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostMaxFrequency(request);
    });

    server->on("/api/ap-mode", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetAPMode(request);
    });

    server->on("/api/ap-mode", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostAPMode(request);
    });

    server->on("/api/save", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostSave(request);
    });

    server->on("/api/load", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostLoad(request);
    });

    // Captive portal detection endpoints
    server->on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });

    server->on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });

    server->on("/mobile/status.php", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });

    server->on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });

    server->on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });

    server->on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Microsoft NCSI");
    });

    // Peripheral API endpoints
    server->on("/api/peripherals", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetPeripheralStatus(request);
    });

    server->on("/api/uart1/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetUART1Status(request);
    });

    server->on("/api/uart1/mode", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostUART1Mode(request);
    });

    server->on("/api/uart1/pwm", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostUART1PWM(request);
    });

    server->on("/api/uart2/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetUART2Status(request);
    });

    server->on("/api/buzzer", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostBuzzer(request);
    });

    server->on("/api/led", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostLEDPWM(request);
    });

    server->on("/api/relay", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostRelay(request);
    });

    server->on("/api/gpio", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostGPIO(request);
    });

    server->on("/api/keys", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetKeys(request);
    });

    // 404 handler
    server->onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found");
    });
}

void WebServerManager::handleGetStatus(AsyncWebServerRequest *request) {
    request->send(200, "application/json", generateStatusJSON());
}

void WebServerManager::handleGetSettings(AsyncWebServerRequest *request) {
    request->send(200, "application/json", generateSettingsJSON());
}

void WebServerManager::handleSetPWMFreq(AsyncWebServerRequest *request) {
    // Motor control now via UART1 (emergency stop removed in v3.0)
    if (!pPeripheralManager) {
        request->send(500, "application/json", "{\"error\":\"Peripheral manager not initialized\"}");
        return;
    }

    if (!request->hasParam("value", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing value parameter\"}");
        return;
    }

    uint32_t freq = request->getParam("value", true)->value().toInt();

    if (pPeripheralManager->getUART1().setPWMFrequency(freq)) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to set frequency\"}");
    }
}

void WebServerManager::handleSetPWMDuty(AsyncWebServerRequest *request) {
    // Motor control now via UART1 (emergency stop removed in v3.0)
    if (!pPeripheralManager) {
        request->send(500, "application/json", "{\"error\":\"Peripheral manager not initialized\"}");
        return;
    }

    if (!request->hasParam("value", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing value parameter\"}");
        return;
    }

    float duty = request->getParam("value", true)->value().toFloat();

    if (pPeripheralManager->getUART1().setPWMDuty(duty)) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to set duty\"}");
    }
}

void WebServerManager::handleMotorStop(AsyncWebServerRequest *request) {
    // Simple stop: set duty to 0% (emergency stop removed in v3.0)
    if (pPeripheralManager) {
        pPeripheralManager->getUART1().setPWMDuty(0.0);
    }
    request->send(200, "application/json", "{\"success\":true}");
}

void WebServerManager::handleClearError(AsyncWebServerRequest *request) {
    // No-op: emergency stop feature removed in v3.0
    request->send(200, "application/json", "{\"success\":true}");
}

void WebServerManager::handleSaveSettings(AsyncWebServerRequest *request) {
    // UART1 settings saved via peripheral manager
    if (pPeripheralManager && pPeripheralManager->saveSettings()) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to save settings\"}");
    }
}

void WebServerManager::handleLoadSettings(AsyncWebServerRequest *request) {
    // UART1 settings loaded via peripheral manager
    if (pPeripheralManager && pPeripheralManager->loadSettings()) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to load settings\"}");
    }
}

void WebServerManager::handleResetSettings(AsyncWebServerRequest *request) {
    // Reset handled by peripheral manager
    // Note: In v3.0, use PERIPHERAL RESET command instead
    request->send(200, "application/json", "{\"success\":true,\"note\":\"Use PERIPHERAL RESET command for full reset\"}");
}

void WebServerManager::handleGetWiFiStatus(AsyncWebServerRequest *request) {
    StaticJsonDocument<256> doc;
    doc["mode"] = pWiFiManager->getModeString();
    doc["status"] = (pWiFiManager->isConnected() ? "connected" : "disconnected");
    doc["ip"] = pWiFiManager->getIPAddress();
    doc["clients"] = pWiFiManager->getClientCount();
    doc["rssi"] = pWiFiManager->getRSSI();

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void WebServerManager::handleScanNetworks(AsyncWebServerRequest *request) {
    int n = pWiFiManager->scanNetworks();

    StaticJsonDocument<2048> doc;
    JsonArray networks = doc.createNestedArray("networks");

    for (int i = 0; i < n && i < 20; i++) {
        String ssid;
        int8_t rssi;
        bool secure;

        if (pWiFiManager->getScanResult(i, ssid, rssi, secure)) {
            JsonObject network = networks.createNestedObject();
            network["ssid"] = ssid;
            network["rssi"] = rssi;
            network["secure"] = secure;
        }
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

String WebServerManager::generateStatusJSON() {
    StaticJsonDocument<1024> doc;

    // Motor control now via UART1 (v3.0)
    if (pPeripheralManager) {
        UART1Mux& uart1 = pPeripheralManager->getUART1();
        doc["rpm"] = uart1.getCalculatedRPM();
        doc["raw_freq"] = uart1.getRPMFrequency();  // Raw frequency instead of raw RPM
        doc["frequency"] = uart1.getPWMFrequency();
        doc["freq"] = uart1.getPWMFrequency();  // Alias for compatibility
        doc["duty"] = uart1.getPWMDuty();
        doc["realInputFrequency"] = uart1.getRPMFrequency();
        doc["input_freq"] = uart1.getRPMFrequency();  // Alias
        // Ramping, emergency stop, and capture_init removed in v3.0
        doc["initialized"] = true;  // Always initialized if peripheral manager exists

        // Format uptime as "H:MM:SS"
        unsigned long uptimeMs = millis();
        unsigned long seconds = uptimeMs / 1000;
        unsigned long hours = seconds / 3600;
        unsigned long minutes = (seconds % 3600) / 60;
        unsigned long secs = seconds % 60;
        char uptimeStr[32];
        sprintf(uptimeStr, "%lu:%02lu:%02lu", hours, minutes, secs);
        doc["uptime"] = String(uptimeStr);

        // Settings from UART1
        doc["polePairs"] = uart1.getPolePairs();
        // Note: ledBrightness moved to StatusLED, rpmUpdateRate moved elsewhere
        // maxFrequency and maxSafeRPM features removed in v3.0
    }

    if (pWiFiManager && pWiFiSettings) {
        doc["wifiConnected"] = pWiFiManager->isConnected();
        doc["wifiIP"] = pWiFiManager->getIPAddress();

        // Check if AP mode is enabled
        bool apMode = (pWiFiSettings->mode == WiFiMode::AP || pWiFiSettings->mode == WiFiMode::AP_STA);
        doc["apModeEnabled"] = apMode;
        doc["apModeActive"] = apMode && (pWiFiManager->getStatus() == WiFiStatus::AP_STARTED);

        // Get AP IP from WiFi library directly
        if (apMode) {
            doc["apIP"] = WiFi.softAPIP().toString();
        } else {
            doc["apIP"] = "";
        }
    }

    // BLE connection status (would need to be passed from main.cpp)
    doc["bleConnected"] = false;  // TODO: Pass BLE status from main

    // System info
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["firmwareVersion"] = "2.1.0";  // TODO: Define version constant

    String json;
    serializeJson(doc, json);
    return json;
}

String WebServerManager::generateSettingsJSON() {
    StaticJsonDocument<512> doc;

    // Motor settings now from UART1 (v3.0)
    if (pPeripheralManager) {
        UART1Mux& uart1 = pPeripheralManager->getUART1();
        doc["frequency"] = uart1.getPWMFrequency();
        doc["duty"] = uart1.getPWMDuty();
        doc["pole_pairs"] = uart1.getPolePairs();
        // max_freq, max_rpm, rpm_update_rate removed in v3.0
    }

    String json;
    serializeJson(doc, json);
    return json;
}

// New API handler implementations for clone

void WebServerManager::handleGetRPM(AsyncWebServerRequest *request) {
    StaticJsonDocument<256> doc;

    // Motor control now via UART1 (v3.0)
    if (pPeripheralManager) {
        UART1Mux& uart1 = pPeripheralManager->getUART1();
        doc["rpm"] = uart1.getCalculatedRPM();
        doc["realInputFrequency"] = uart1.getRPMFrequency();
        doc["polePairs"] = uart1.getPolePairs();
        doc["frequency"] = uart1.getPWMFrequency();
        doc["duty"] = uart1.getPWMDuty();
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void WebServerManager::handleGetConfig(AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;

    // v3.0: Motor control via UART1, many settings removed
    if (pPeripheralManager) {
        UART1Mux& uart1 = pPeripheralManager->getUART1();

        // UI configuration
        doc["title"] = "ESP32-S3 Motor Control v3.0";
        doc["subtitle"] = "PWM & RPM Monitoring (UART1)";
        // language, chartUpdateRate, ledBrightness moved to WiFi settings or removed
        doc["language"] = "en";  // Default

        // Motor settings from UART1
        doc["polePairs"] = uart1.getPolePairs();
        // maxFrequency, maxSafeRPM, maxSafeRPMEnabled removed in v3.0
        doc["frequency"] = uart1.getPWMFrequency();  // Current actual frequency
        doc["duty"] = uart1.getPWMDuty();            // Current actual duty

        // Also send RPM for initial display
        doc["rpm"] = uart1.getCalculatedRPM();
        doc["realInputFrequency"] = uart1.getRPMFrequency();

        // Check if AP mode is enabled
        if (pWiFiSettings) {
            bool apMode = (pWiFiSettings->mode == WiFiMode::AP || pWiFiSettings->mode == WiFiMode::AP_STA);
            doc["apModeEnabled"] = apMode;

            // WiFi Station settings - send empty string if not set
            if (strlen(pWiFiSettings->sta_ssid) > 0) {
                doc["wifiSSID"] = pWiFiSettings->sta_ssid;
            } else {
                doc["wifiSSID"] = "";
            }
            // Password is always sent as empty (for security)
            doc["wifiPassword"] = "";
        } else {
            doc["apModeEnabled"] = false;
            doc["wifiSSID"] = "";
            doc["wifiPassword"] = "";
        }

        // BLE device name - hardcoded for now, will be configurable in future
        doc["bleDeviceName"] = "ESP32-S3 Motor Control";
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void WebServerManager::handlePostConfig(AsyncWebServerRequest *request) {
    // This handler is called after body is parsed
    // Check if we have JSON body data
    bool updated = false;
    String updateMessage = "";

    // v3.0: LED brightness and chart update settings moved to WiFi/UI settings
    // (ledBrightness now in StatusLED, chartUpdateRate/rpmUpdateRate in UI config)
    if (request->hasParam("ledBrightness", true)) {
        uint8_t brightness = request->getParam("ledBrightness", true)->value().toInt();
        if (pStatusLED) {
            pStatusLED->setBrightness(brightness);
        }
        updated = true;
    }

    // Handle JSON body (from settings page)
    if (request->_tempObject != nullptr) {
        String* bodyStr = (String*)request->_tempObject;

        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, *bodyStr);

        if (!error) {
            // v3.0: Language settings removed (was in MotorSettings)
            // LED brightness now handled directly via StatusLED
            if (doc.containsKey("ledBrightness")) {
                uint8_t brightness = doc["ledBrightness"];
                // Apply brightness to actual LED immediately
                if (pStatusLED) {
                    pStatusLED->setBrightness(brightness);
                    Serial.printf("✅ LED brightness updated and applied: %d\n", brightness);
                }
                updated = true;
            }

            // rpmUpdateRate removed in v3.0 (was UI-only setting)

            // Motor settings now via UART1 (v3.0)
            if (doc.containsKey("polePairs") && pPeripheralManager) {
                uint8_t polePairs = doc["polePairs"];
                pPeripheralManager->getUART1().setPolePairs(polePairs);
                Serial.printf("✅ Pole pairs set to: %d\n", polePairs);
                updated = true;
            }

            // maxFrequency, maxSafeRPM, maxSafeRPMEnabled removed in v3.0

            // WiFi settings
            if (doc.containsKey("wifiSSID") || doc.containsKey("wifiPassword")) {
                bool wifiUpdated = false;

                if (pWiFiSettingsManager) {
                    WiFiSettings& wifiSettings = pWiFiSettingsManager->get();

                    if (doc.containsKey("wifiSSID")) {
                        const char* ssid = doc["wifiSSID"];
                        Serial.printf("📡 WiFi SSID received: %s\n", ssid);
                        strncpy(wifiSettings.sta_ssid, ssid, sizeof(wifiSettings.sta_ssid) - 1);
                        wifiSettings.sta_ssid[sizeof(wifiSettings.sta_ssid) - 1] = '\0';
                        wifiUpdated = true;
                    }

                    if (doc.containsKey("wifiPassword")) {
                        const char* password = doc["wifiPassword"];
                        Serial.println("📡 WiFi password received");
                        // Only update password if not empty (empty means keep existing)
                        if (password && strlen(password) > 0) {
                            strncpy(wifiSettings.sta_password, password, sizeof(wifiSettings.sta_password) - 1);
                            wifiSettings.sta_password[sizeof(wifiSettings.sta_password) - 1] = '\0';
                            wifiUpdated = true;
                        }
                    }

                    if (wifiUpdated) {
                        if (pWiFiSettingsManager->save()) {
                            Serial.println("💾 WiFi settings saved to NVS");
                            updateMessage += "WiFi settings saved. ";
                        } else {
                            Serial.println("❌ Failed to save WiFi settings");
                            updateMessage += "WiFi settings save failed. ";
                        }
                    }
                } else {
                    Serial.println("⚠️ WiFiSettingsManager not available");
                    updateMessage += "WiFi settings manager not available. ";
                }
            }

            // BLE device name
            if (doc.containsKey("bleDeviceName")) {
                const char* bleName = doc["bleDeviceName"];
                Serial.printf("📶 BLE device name received: %s\n", bleName);
                // Note: BLE name requires BLEDevice access to update
                // TODO: Update BLE device name dynamically
                updateMessage += "BLE name requires implementation. ";
            }

            // Save settings to NVS if anything changed (v3.0: via peripheral manager)
            if (updated && pPeripheralManager) {
                pPeripheralManager->saveSettings();
                Serial.println("💾 Settings saved to NVS");
            }
        }

        delete bodyStr;
        request->_tempObject = nullptr;
    }

    if (updated) {
        String response = "{\"success\":true,\"message\":\"Configuration updated";
        if (updateMessage.length() > 0) {
            response += " (" + updateMessage + ")";
        }
        response += "\"}";
        request->send(200, "application/json", response);
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"No valid parameters\"}");
    }
}

void WebServerManager::handlePostPWM(AsyncWebServerRequest *request) {
    bool hasFreq = request->hasParam("frequency", true);
    bool hasDuty = request->hasParam("duty", true);

    if (!hasFreq && !hasDuty) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing frequency or duty parameter\"}");
        return;
    }

    bool success = true;
    String message = "";

    // v3.0: Motor control via UART1
    if (hasFreq && pPeripheralManager) {
        uint32_t freq = request->getParam("frequency", true)->value().toInt();
        if (pPeripheralManager->getUART1().setPWMFrequency(freq)) {
            message += "Frequency: " + String(freq) + "Hz ";
        } else {
            success = false;
        }
    }

    if (hasDuty && pPeripheralManager) {
        float duty = request->getParam("duty", true)->value().toFloat();
        if (pPeripheralManager->getUART1().setPWMDuty(duty)) {
            message += "Duty: " + String(duty, 1) + "%";
        } else {
            success = false;
        }
    }

    if (success) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"" + message + "\"}");
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to set PWM parameters\"}");
    }
}

void WebServerManager::handlePostPolePairs(AsyncWebServerRequest *request) {
    if (!request->hasParam("polePairs", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing polePairs parameter\"}");
        return;
    }

    uint8_t polePairs = request->getParam("polePairs", true)->value().toInt();

    if (polePairs < 1 || polePairs > 12) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Pole pairs must be between 1 and 12\"}");
        return;
    }

    // v3.0: Update pole pairs via UART1
    if (pPeripheralManager) {
        pPeripheralManager->getUART1().setPolePairs(polePairs);
        request->send(200, "application/json", "{\"success\":true,\"polePairs\":" + String(polePairs) + "}");
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Peripheral manager not initialized\"}");
    }
}

void WebServerManager::handlePostMaxFrequency(AsyncWebServerRequest *request) {
    // v3.0: maxFrequency feature removed (was for limiting motor speed)
    request->send(200, "application/json", "{\"success\":true,\"note\":\"maxFrequency feature removed in v3.0\"}");
}

void WebServerManager::handleGetAPMode(AsyncWebServerRequest *request) {
    StaticJsonDocument<256> doc;

    if (pWiFiSettings && pWiFiManager) {
        // Check if AP mode is enabled in settings
        bool apMode = (pWiFiSettings->mode == WiFiMode::AP || pWiFiSettings->mode == WiFiMode::AP_STA);
        doc["enabled"] = apMode;
        doc["active"] = apMode && (pWiFiManager->getStatus() == WiFiStatus::AP_STARTED);

        // Get AP IP and SSID
        if (apMode) {
            doc["ip"] = WiFi.softAPIP().toString();
            doc["ssid"] = String(pWiFiSettings->ap_ssid);
        } else {
            doc["ip"] = "";
            doc["ssid"] = "";
        }
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void WebServerManager::handlePostAPMode(AsyncWebServerRequest *request) {
    if (!request->hasParam("enabled", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing enabled parameter\"}");
        return;
    }

    String enabledStr = request->getParam("enabled", true)->value();
    bool enabled = (enabledStr == "true" || enabledStr == "1");

    // TODO: Implement AP mode control in WiFiManager
    // For now, just acknowledge the request

    request->send(200, "application/json", "{\"success\":true,\"enabled\":" + String(enabled ? "true" : "false") + "}");
}

void WebServerManager::handlePostSave(AsyncWebServerRequest *request) {
    // v3.0: Settings saved via peripheral manager
    if (pPeripheralManager && pPeripheralManager->saveSettings()) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Settings saved to NVS\"}");
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save settings\"}");
    }
}

void WebServerManager::handlePostLoad(AsyncWebServerRequest *request) {
    // v3.0: Settings loaded via peripheral manager
    if (pPeripheralManager && pPeripheralManager->loadSettings()) {
        // Return the loaded values so the web interface can update
        UART1Mux& uart1 = pPeripheralManager->getUART1();

        StaticJsonDocument<256> doc;
        doc["success"] = true;
        doc["message"] = "Settings loaded from NVS";
        doc["frequency"] = uart1.getPWMFrequency();
        doc["duty"] = uart1.getPWMDuty();
        doc["polePairs"] = uart1.getPolePairs();
        // maxFrequency, ledBrightness, rpmUpdateRate removed in v3.0

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to load settings\"}");
    }
}

String WebServerManager::generateIndexHTML() {
    // HTML will be defined in a separate section below
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Motor Control</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            border-radius: 15px;
            padding: 30px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.3);
        }
        h1 {
            color: #667eea;
            margin-bottom: 10px;
            text-align: center;
        }
        .status {
            background: #f7f7f7;
            padding: 15px;
            border-radius: 10px;
            margin: 20px 0;
        }
        .status-row {
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid #ddd;
        }
        .status-row:last-child { border-bottom: none; }
        .status-label { font-weight: bold; color: #555; }
        .status-value { color: #333; font-family: monospace; }
        .rpm-display {
            text-align: center;
            padding: 30px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border-radius: 15px;
            margin: 20px 0;
        }
        .rpm-value {
            font-size: 48px;
            font-weight: bold;
            margin: 10px 0;
        }
        .rpm-label {
            font-size: 18px;
            opacity: 0.9;
        }
        .control-panel {
            margin: 20px 0;
        }
        .control-group {
            margin: 15px 0;
            padding: 15px;
            background: #f7f7f7;
            border-radius: 10px;
        }
        .control-group label {
            display: block;
            margin-bottom: 8px;
            font-weight: bold;
            color: #555;
        }
        input[type="range"] {
            width: 100%;
            height: 8px;
            border-radius: 5px;
            background: #ddd;
            outline: none;
        }
        input[type="number"] {
            width: 100%;
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 5px;
            font-size: 16px;
        }
        .value-display {
            text-align: right;
            margin-top: 5px;
            font-size: 18px;
            font-weight: bold;
            color: #667eea;
        }
        button {
            width: 100%;
            padding: 12px 20px;
            margin: 10px 0;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.3s;
        }
        .btn-primary {
            background: #667eea;
            color: white;
        }
        .btn-primary:hover {
            background: #5568d3;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
        }
        .btn-danger {
            background: #e74c3c;
            color: white;
        }
        .btn-danger:hover {
            background: #c0392b;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(231, 76, 60, 0.4);
        }
        .btn-success {
            background: #27ae60;
            color: white;
        }
        .btn-success:hover {
            background: #229954;
            transform: translateY(-2px);
        }
        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
        }
        .status-connected { background: #27ae60; }
        .status-disconnected { background: #e74c3c; }
        .error-banner {
            background: rgba(231, 76, 60, 0.15);
            border: 2px solid #e74c3c;
            border-radius: 12px;
            padding: 15px 20px;
            margin: 20px 0;
            display: none;
            animation: pulse 2s ease-in-out infinite;
        }
        .error-banner.show {
            display: block;
        }
        .error-banner-content {
            display: flex;
            align-items: center;
            justify-content: space-between;
            flex-wrap: wrap;
            gap: 15px;
        }
        .error-banner-text {
            color: #e74c3c;
            font-weight: bold;
            font-size: 16px;
            flex: 1;
            min-width: 200px;
        }
        .error-banner-icon {
            font-size: 24px;
            margin-right: 10px;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.7; }
        }
        .btn-warning {
            background: #f39c12;
            color: white;
        }
        .btn-warning:hover {
            background: #e67e22;
            transform: translateY(-2px);
        }
        .footer {
            text-align: center;
            margin-top: 30px;
            color: #999;
            font-size: 14px;
        }
        @media (max-width: 600px) {
            .container { padding: 20px; }
            .rpm-value { font-size: 36px; }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔧 ESP32 Motor Control</h1>
        <p style="text-align: center; color: #999; margin-bottom: 20px;">
            <span class="status-indicator" id="wsStatus"></span>
            <span id="wsStatusText">Connecting...</span>
        </p>

        <!-- Emergency Stop Error Banner -->
        <div class="error-banner" id="errorBanner">
            <div class="error-banner-content">
                <div>
                    <span class="error-banner-icon">⛔</span>
                    <span class="error-banner-text">SAFETY ALERT: Emergency stop activated! Motor is stopped.</span>
                </div>
                <button class="btn-warning" onclick="clearError()">Clear Error / Resume</button>
            </div>
        </div>

        <div class="rpm-display">
            <div class="rpm-label">Current RPM</div>
            <div class="rpm-value" id="rpmValue">0</div>
            <div class="rpm-label">Frequency: <span id="inputFreq">0</span> Hz</div>
        </div>

        <div class="status">
            <div class="status-row">
                <span class="status-label">PWM Frequency:</span>
                <span class="status-value" id="pwmFreq">0 Hz</span>
            </div>
            <div class="status-row">
                <span class="status-label">PWM Duty Cycle:</span>
                <span class="status-value" id="pwmDuty">0%</span>
            </div>
            <div class="status-row">
                <span class="status-label">Ramping:</span>
                <span class="status-value" id="rampStatus">No</span>
            </div>
            <div class="status-row">
                <span class="status-label">Uptime:</span>
                <span class="status-value" id="uptime">0s</span>
            </div>
        </div>

        <div class="control-panel">
            <div class="control-group">
                <label for="freqInput">PWM Frequency (Hz)</label>
                <input type="number" id="freqInput" min="10" max="500000" value="15000">
                <button class="btn-primary" onclick="setFrequency()">Set Frequency</button>
            </div>

            <div class="control-group">
                <label for="dutySlider">PWM Duty Cycle: <span id="dutyDisplay">0%</span></label>
                <input type="range" id="dutySlider" min="0" max="100" value="0" step="0.1" oninput="updateDutyDisplay()">
                <button class="btn-primary" onclick="setDuty()">Set Duty Cycle</button>
            </div>

            <button class="btn-danger" onclick="emergencyStop()">⛔ EMERGENCY STOP</button>
            <button class="btn-success" onclick="saveSettings()">💾 Save Settings</button>
        </div>

        <div class="footer">
            ESP32-S3 Motor Control System v2.4<br>
            WebSocket Status: <span id="wsConnStatus">Disconnected</span>
        </div>
    </div>

    <script>
        let ws;
        let reconnectInterval;

        function connectWebSocket() {
            ws = new WebSocket('ws://' + window.location.hostname + '/ws');

            ws.onopen = function() {
                console.log('WebSocket connected');
                document.getElementById('wsStatus').className = 'status-indicator status-connected';
                document.getElementById('wsStatusText').textContent = 'Connected';
                document.getElementById('wsConnStatus').textContent = 'Connected';
                clearInterval(reconnectInterval);

                // Request initial status
                ws.send(JSON.stringify({cmd: 'get_status'}));
            };

            ws.onclose = function() {
                console.log('WebSocket disconnected');
                document.getElementById('wsStatus').className = 'status-indicator status-disconnected';
                document.getElementById('wsStatusText').textContent = 'Disconnected';
                document.getElementById('wsConnStatus').textContent = 'Disconnected';

                // Attempt reconnect every 3 seconds
                reconnectInterval = setInterval(connectWebSocket, 3000);
            };

            ws.onerror = function(error) {
                console.error('WebSocket error:', error);
            };

            ws.onmessage = function(event) {
                try {
                    const data = JSON.parse(event.data);
                    handleMessage(data);
                } catch (e) {
                    console.error('Parse error:', e);
                }
            };
        }

        function handleMessage(data) {
            if (data.type === 'status' || data.type === 'rpm') {
                if (data.rpm !== undefined) {
                    document.getElementById('rpmValue').textContent = Math.round(data.rpm);
                }
                if (data.freq !== undefined) {
                    document.getElementById('pwmFreq').textContent = data.freq + ' Hz';
                    document.getElementById('freqInput').value = data.freq;
                }
                if (data.duty !== undefined) {
                    document.getElementById('pwmDuty').textContent = data.duty.toFixed(1) + '%';
                    document.getElementById('dutySlider').value = data.duty;
                    document.getElementById('dutyDisplay').textContent = data.duty.toFixed(1) + '%';
                }
                if (data.raw_rpm !== undefined) {
                    const inputFreq = data.raw_rpm / 60; // Approximate
                    document.getElementById('inputFreq').textContent = inputFreq.toFixed(1);
                }
                if (data.ramping !== undefined) {
                    document.getElementById('rampStatus').textContent = data.ramping ? 'Yes' : 'No';
                }
                if (data.uptime !== undefined) {
                    const seconds = Math.floor(data.uptime / 1000);
                    document.getElementById('uptime').textContent = seconds + 's';
                }
                // Handle emergency stop status
                if (data.emergencyStop !== undefined) {
                    const errorBanner = document.getElementById('errorBanner');
                    if (data.emergencyStop) {
                        errorBanner.classList.add('show');
                    } else {
                        errorBanner.classList.remove('show');
                    }
                }
            }
        }

        function setFrequency() {
            const freq = parseInt(document.getElementById('freqInput').value);
            if (freq >= 10 && freq <= 500000) {
                ws.send(JSON.stringify({cmd: 'set_freq', value: freq}));
            } else {
                alert('Frequency must be between 10 and 500000 Hz');
            }
        }

        function setDuty() {
            const duty = parseFloat(document.getElementById('dutySlider').value);
            ws.send(JSON.stringify({cmd: 'set_duty', value: duty}));
        }

        function emergencyStop() {
            if (confirm('Are you sure you want to emergency stop the motor?')) {
                ws.send(JSON.stringify({cmd: 'stop'}));
            }
        }

        function clearError() {
            if (confirm('Clear emergency stop and resume normal operation?')) {
                ws.send(JSON.stringify({cmd: 'clear_error'}));
            }
        }

        function saveSettings() {
            fetch('/api/settings/save', {method: 'POST'})
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert('Settings saved successfully!');
                    } else {
                        alert('Failed to save settings');
                    }
                })
                .catch(error => {
                    console.error('Error:', error);
                    alert('Error saving settings');
                });
        }

        function updateDutyDisplay() {
            const duty = parseFloat(document.getElementById('dutySlider').value);
            document.getElementById('dutyDisplay').textContent = duty.toFixed(1) + '%';
        }

        // Initialize
        connectWebSocket();
        updateDutyDisplay();
    </script>
</body>
</html>
)rawliteral";
}
