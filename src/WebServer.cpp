#include "WebServer.h"
#include "ArduinoJson.h"

WebServerManager::WebServerManager() {
    // Constructor
}

bool WebServerManager::begin(WiFiSettings* wifiSettings,
                             MotorControl* motorControl,
                             MotorSettingsManager* motorSettingsManager,
                             WiFiManager* wifiManager) {
    if (!wifiSettings || !motorControl || !motorSettingsManager || !wifiManager) {
        Serial.println("❌ WebServerManager::begin() - NULL pointer!");
        return false;
    }

    pWiFiSettings = wifiSettings;
    pMotorControl = motorControl;
    pMotorSettingsManager = motorSettingsManager;
    pWiFiManager = wifiManager;

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
    if (!ws || ws->count() == 0 || !pMotorControl) {
        return;
    }

    StaticJsonDocument<512> doc;
    doc["type"] = "status";
    doc["rpm"] = pMotorControl->getCurrentRPM();
    doc["raw_rpm"] = pMotorControl->getRawRPM();
    doc["freq"] = pMotorControl->getPWMFrequency();
    doc["duty"] = pMotorControl->getPWMDuty();
    doc["ramping"] = pMotorControl->isRamping();
    doc["uptime"] = pMotorControl->getUptime();

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

        // Parse JSON command
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, message);

        if (error) {
            Serial.printf("[WS] JSON parse error: %s\n", error.c_str());
            return;
        }

        const char* cmd = doc["cmd"];
        if (!cmd) {
            return;
        }

        // Handle commands
        if (strcmp(cmd, "set_freq") == 0) {
            uint32_t freq = doc["value"];
            pMotorControl->setPWMFrequency(freq);
            broadcastStatus();
        }
        else if (strcmp(cmd, "set_duty") == 0) {
            float duty = doc["value"];
            pMotorControl->setPWMDuty(duty);
            broadcastStatus();
        }
        else if (strcmp(cmd, "stop") == 0) {
            pMotorControl->emergencyStop();
            broadcastStatus();
        }
        else if (strcmp(cmd, "get_status") == 0) {
            broadcastStatus();
        }
    }
}

void WebServerManager::setupRoutes() {
    // Serve main page
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", generateIndexHTML());
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
    if (!request->hasParam("value", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing value parameter\"}");
        return;
    }

    uint32_t freq = request->getParam("value", true)->value().toInt();

    if (pMotorControl->setPWMFrequency(freq)) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to set frequency\"}");
    }
}

void WebServerManager::handleSetPWMDuty(AsyncWebServerRequest *request) {
    if (!request->hasParam("value", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing value parameter\"}");
        return;
    }

    float duty = request->getParam("value", true)->value().toFloat();

    if (pMotorControl->setPWMDuty(duty)) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to set duty\"}");
    }
}

void WebServerManager::handleMotorStop(AsyncWebServerRequest *request) {
    pMotorControl->emergencyStop();
    request->send(200, "application/json", "{\"success\":true}");
}

void WebServerManager::handleSaveSettings(AsyncWebServerRequest *request) {
    if (pMotorSettingsManager->save()) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to save settings\"}");
    }
}

void WebServerManager::handleLoadSettings(AsyncWebServerRequest *request) {
    if (pMotorSettingsManager->load()) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to load settings\"}");
    }
}

void WebServerManager::handleResetSettings(AsyncWebServerRequest *request) {
    pMotorSettingsManager->reset();
    pMotorSettingsManager->save();
    request->send(200, "application/json", "{\"success\":true}");
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
    StaticJsonDocument<512> doc;

    if (pMotorControl) {
        doc["rpm"] = pMotorControl->getCurrentRPM();
        doc["raw_rpm"] = pMotorControl->getRawRPM();
        doc["freq"] = pMotorControl->getPWMFrequency();
        doc["duty"] = pMotorControl->getPWMDuty();
        doc["input_freq"] = pMotorControl->getInputFrequency();
        doc["ramping"] = pMotorControl->isRamping();
        doc["initialized"] = pMotorControl->isInitialized();
        doc["capture_init"] = pMotorControl->isCaptureInitialized();
        doc["uptime"] = pMotorControl->getUptime();
    }

    String json;
    serializeJson(doc, json);
    return json;
}

String WebServerManager::generateSettingsJSON() {
    StaticJsonDocument<512> doc;

    if (pMotorSettingsManager) {
        const MotorSettings& settings = pMotorSettingsManager->get();
        doc["frequency"] = settings.frequency;
        doc["duty"] = settings.duty;
        doc["pole_pairs"] = settings.polePairs;
        doc["max_freq"] = settings.maxFrequency;
        doc["max_rpm"] = settings.maxSafeRPM;
        doc["rpm_update_rate"] = settings.rpmUpdateRate;
    }

    String json;
    serializeJson(doc, json);
    return json;
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
