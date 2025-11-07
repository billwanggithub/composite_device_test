#include <Arduino.h>
#include "USB.h"
#include "USBCDC.h"
#include "CustomHID.h"
#include "CommandParser.h"
#include "HIDProtocol.h"
#include "MotorControl.h"
#include "MotorSettings.h"
#include "StatusLED.h"
#include "WiFiSettings.h"
#include "WiFiManager.h"
#include "WebServer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <SPIFFS.h>

// USB CDC 實例（用於 console）
USBCDC USBSerial;

// 自訂 HID 實例（64 位元組，無 Report ID）
CustomHID64 HID;

// BLE 相關定義
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a9"

BLEServer* pBLEServer = nullptr;
BLECharacteristic* pTxCharacteristic = nullptr;
BLECharacteristic* pRxCharacteristic = nullptr;
bool bleDeviceConnected = false;
bool bleOldDeviceConnected = false;
// Queue for pending BLE notifications; stores heap-allocated char* messages
QueueHandle_t bleNotifyQueue = nullptr;

// HID 資料結構
typedef struct {
    uint8_t data[64];
    uint16_t len;
    uint8_t report_id;
    uint16_t raw_len;
} HIDDataPacket;

// BLE 命令結構
typedef struct {
    char command[256];  // BLE 命令字串（最大 255 字元 + null terminator）
    uint16_t len;       // 命令長度
} BLECommandPacket;

// FreeRTOS 資源
QueueHandle_t hidDataQueue = nullptr;      // HID 資料佇列
QueueHandle_t bleCommandQueue = nullptr;   // BLE 命令佇列
SemaphoreHandle_t serialMutex = nullptr;   // 保護 USBSerial 存取
SemaphoreHandle_t bufferMutex = nullptr;   // 保護 hid_out_buffer 存取
SemaphoreHandle_t hidSendMutex = nullptr;  // 保護 HID.send() 存取

// HID 緩衝區（由 mutex 保護）
uint8_t hid_out_buffer[64] = {0};
bool hid_data_ready = false;

// 命令解析器（共用）
CommandParser parser;
CDCResponse* cdc_response = nullptr;
HIDResponse* hid_response = nullptr;
MultiChannelResponse* multi_response = nullptr;  // 多通道回應（同時輸出到 HID 和 CDC）
BLEResponse* ble_response = nullptr;

// HID 命令緩衝區
String hid_command_buffer = "";

// Motor control instances
MotorControl motorControl;
MotorSettingsManager motorSettingsManager;

// Status LED instance
StatusLED statusLED;

// WiFi and Web Server instances
WiFiSettingsManager wifiSettingsManager;
WiFiManager wifiManager;
WebServerManager webServerManager;

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        bleDeviceConnected = true;
        USBSerial.println("[BLE] 客戶端已連接");
        // Flush any queued notifications
        if (bleNotifyQueue) {
            char* msg = nullptr;
            while (xQueueReceive(bleNotifyQueue, &msg, 0) == pdTRUE) {
                if (msg) {
                    if (pTxCharacteristic) {
                        pTxCharacteristic->setValue((uint8_t*)msg, strlen(msg));
                        pTxCharacteristic->notify();
                        delay(10);
                    }
                    free(msg);
                }
            }
        }
    }

    void onDisconnect(BLEServer* pServer) {
        bleDeviceConnected = false;
        USBSerial.println("[BLE] 客戶端已斷開");
        // 重新開始廣播，允許其他客戶端連接
        delay(500);  // 短暫延遲確保斷開完成
        pServer->startAdvertising();
        USBSerial.println("[BLE] 重新開始廣播");
    }
};

// BLE RX Characteristic Callbacks (接收來自客戶端的命令)
class MyRxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0 && rxValue.length() < 256) {
            // 創建命令封包
            BLECommandPacket packet;
            strncpy(packet.command, rxValue.c_str(), sizeof(packet.command) - 1);
            packet.command[sizeof(packet.command) - 1] = '\0';  // 確保 null terminator
            packet.len = rxValue.length();

            // 放入佇列（不阻塞，從回調中調用）
            if (bleCommandQueue) {
                BaseType_t result = xQueueSend(bleCommandQueue, &packet, 0);
                if (result != pdTRUE) {
                    // 佇列已滿，丟棄命令
                    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10))) {
                        USBSerial.println("[BLE] 命令佇列已滿，命令被丟棄");
                        xSemaphoreGive(serialMutex);
                    }
                }
            }
        }
    }
};

// HID 資料接收回調函數（在 ISR 上下文中執行）
void onHIDData(const uint8_t* data, uint16_t len) {
    if (len <= 64) {
        // 準備資料包
        HIDDataPacket packet;
        memcpy(packet.data, data, len);
        packet.len = len;
        packet.report_id = HID.getLastReportId();
        packet.raw_len = HID.getLastRawLen();

        // 從 ISR 發送到佇列（不阻塞）
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(hidDataQueue, &packet, &xHigherPriorityTaskWoken);

        // 如果需要，進行任務切換
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

// HID 處理 Task
void hidTask(void* parameter) {
    HIDDataPacket packet;
    char command_buffer[65] = {0};  // 最多 64 bytes 命令 + null terminator
    uint8_t command_len = 0;
    bool is_0xA1_protocol = false;

    while (true) {
        // 等待 HID 資料
        if (xQueueReceive(hidDataQueue, &packet, portMAX_DELAY)) {

            // 自動偵測並解析命令（支援 0xA1 協定和純文本協定）
            if (HIDProtocol::parseCommand(packet.data, command_buffer, &command_len, &is_0xA1_protocol)) {
                // ========== 這是命令封包 ==========
                if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
                    const char* protocol_name = is_0xA1_protocol ? "0xA1" : "純文本";
                    USBSerial.printf("\n[HID CMD %s] %s\n", protocol_name, command_buffer);
                    xSemaphoreGive(serialMutex);
                }

                // 執行命令（根據命令類型路由回應）
                String cmd_str(command_buffer);

                // SCPI 命令 → 只回應到 HID
                // 一般命令 → 只回應到 CDC
                if (CommandParser::isSCPICommand(cmd_str)) {
                    parser.processCommand(cmd_str, hid_response, CMD_SOURCE_HID);
                } else {
                    parser.processCommand(cmd_str, cdc_response, CMD_SOURCE_HID);
                }

                // 顯示提示符
                if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
                    if (USBSerial) {
                        USBSerial.print("> ");
                    }
                    xSemaphoreGive(serialMutex);
                }

            } else {
                // ========== 這是原始資料（非命令）==========
                // 更新共享緩衝區（加鎖）
                if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
                    memcpy(hid_out_buffer, packet.data, packet.len);
                    hid_data_ready = true;
                    xSemaphoreGive(bufferMutex);
                }

                // 顯示除錯資訊（加鎖保護 USBSerial）
                if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
                    USBSerial.printf("\n[DEBUG] HID OUT (原始資料): %d 位元組\n", packet.len);

                    // 顯示前 16 bytes
                    USBSerial.print("前16: ");
                    for (uint16_t i = 0; i < packet.len && i < 16; i++) {
                        USBSerial.printf("%02X ", packet.data[i]);
                    }
                    USBSerial.println();

                    // 顯示最後 16 bytes
                    if (packet.len > 16) {
                        uint16_t start = packet.len - 16;
                        USBSerial.print("後16: ");
                        for (uint16_t i = start; i < packet.len; i++) {
                            USBSerial.printf("%02X ", packet.data[i]);
                        }
                        USBSerial.println();
                    }
                    USBSerial.print("> ");

                    xSemaphoreGive(serialMutex);
                }
            }
        }
    }
}

// CDC 處理 Task
void cdcTask(void* parameter) {
    String cdc_command_buffer = "";

    while (true) {
        // 檢查是否有可用資料（使用 mutex 保護）
        int available = 0;
        if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
            available = USBSerial.available();
            xSemaphoreGive(serialMutex);
        }

        // 處理 CDC 輸入
        while (available > 0) {
            char c = 0;

            // 讀取單個字元（使用 mutex 保護）
            if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
                if (USBSerial.available()) {
                    c = USBSerial.read();
                    available = USBSerial.available();  // 更新剩餘可用資料數量
                } else {
                    available = 0;  // 沒有資料了
                }
                xSemaphoreGive(serialMutex);
            } else {
                break;  // 無法獲取 mutex，跳出迴圈
            }

            // 處理接收到的字元（在 mutex 外部，不影響其他 task）
            if (c == '\n' || c == '\r') {
                // 收到換行符，處理完整命令
                if (cdc_command_buffer.length() > 0) {
                    // 取得 mutex 保護 USBSerial 輸出
                    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(1000))) {
                        // 處理命令（CDC 命令只輸出到 CDC）
                        parser.processCommand(cdc_command_buffer, cdc_response, CMD_SOURCE_CDC);
                        cdc_command_buffer = "";  // 清空緩衝區

                        // 顯示提示符
                        USBSerial.print("> ");
                        xSemaphoreGive(serialMutex);
                    }
                }
            } else if (c == '\b' || c == 127) {
                // 退格鍵
                if (cdc_command_buffer.length() > 0) {
                    cdc_command_buffer.remove(cdc_command_buffer.length() - 1);
                }
            } else if (c >= 32 && c < 127) {
                // 可列印字元
                cdc_command_buffer += c;
            }
        }

        // 短暫延遲避免佔用 CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// BLE 處理 Task
void bleTask(void* parameter) {
    BLECommandPacket packet;

    while (true) {
        // 從佇列接收命令（阻塞等待）
        if (xQueueReceive(bleCommandQueue, &packet, portMAX_DELAY) == pdTRUE) {
            String command = String(packet.command);
            command.trim();

            // 調試輸出（保護 USBSerial）
            if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
                USBSerial.printf("\n[BLE CMD] %s\n", command.c_str());
                xSemaphoreGive(serialMutex);
            }

            // 處理 BLE 命令（根據命令類型路由回應）
            // 重要：在任務上下文中調用，不在 BLE 回調中！
            // SCPI 命令 → 只回應到 BLE
            // 一般命令 → 只回應到 CDC
            if (CommandParser::isSCPICommand(command)) {
                parser.processCommand(command, ble_response, CMD_SOURCE_BLE);
            } else {
                parser.processCommand(command, cdc_response, CMD_SOURCE_BLE);
            }

            // 短暫延遲確保 BLE 通知發送完成
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// WiFi 處理 Task
void wifiTask(void* parameter) {
    TickType_t lastWiFiUpdate = 0;
    TickType_t lastWebUpdate = 0;

    while (true) {
        TickType_t now = xTaskGetTickCount();

        // Update WiFi status (check connection, handle reconnection)
        if (now - lastWiFiUpdate >= pdMS_TO_TICKS(1000)) {  // Every 1 second
            wifiManager.update();
            lastWiFiUpdate = now;
        }

        // Update web server (WebSocket broadcasts, cleanup)
        if (now - lastWebUpdate >= pdMS_TO_TICKS(200)) {  // Every 200ms (5 Hz)
            if (webServerManager.isRunning()) {
                webServerManager.update();
            }
            lastWebUpdate = now;
        }

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms loop rate
    }
}

// Motor 處理 Task
void motorTask(void* parameter) {
    TickType_t lastRPMUpdate = 0;
    TickType_t lastSafetyCheck = 0;
    TickType_t lastLEDUpdate = 0;
    TickType_t lastWatchdogFeed = 0;

    while (true) {
        TickType_t now = xTaskGetTickCount();

        // Update PWM ramping (high frequency for smooth transitions)
        motorControl.updateRamping();

        // Update RPM reading
        if (now - lastRPMUpdate >= pdMS_TO_TICKS(motorSettingsManager.get().rpmUpdateRate)) {
            motorControl.updateRPM();
            lastRPMUpdate = now;
        }

        // Feed watchdog every 100ms
        if (now - lastWatchdogFeed >= pdMS_TO_TICKS(100)) {
            motorControl.feedWatchdog();
            lastWatchdogFeed = now;
        }

        // Safety check every 500ms
        if (now - lastSafetyCheck >= pdMS_TO_TICKS(500)) {
            bool safetyOK = motorControl.checkSafety();
            bool watchdogOK = motorControl.checkWatchdog();

            if (!safetyOK || !watchdogOK) {
                // Emergency stop triggered
                if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10))) {
                    if (!safetyOK) {
                        USBSerial.println("\n⚠️ SAFETY ALERT: Emergency stop activated!");
                    }
                    if (!watchdogOK) {
                        USBSerial.println("\n⚠️ WATCHDOG ALERT: Timeout detected - emergency stop!");
                    }
                    xSemaphoreGive(serialMutex);
                }
                motorControl.emergencyStop();
                // Set LED to FAST blinking red (error) - 100ms for urgent warning
                statusLED.blinkRed(100);
            }
            lastSafetyCheck = now;
        }

        // Update LED based on system state every 1 second
        if (now - lastLEDUpdate >= pdMS_TO_TICKS(1000)) {
            // Priority 1: Check for error states (handled in safety check above with fast red blink)
            // Priority 2: Web server not ready - blink orange/yellow as warning
            if (!webServerManager.isRunning()) {
                // Web server not ready - blink yellow to indicate waiting for web server
                statusLED.blinkYellow(500);
            }
            // Priority 3: Normal operation states
            else if (bleDeviceConnected) {
                // BLE connected - purple
                statusLED.setPurple();
            } else if (motorControl.isRamping()) {
                // Ramping active - yellow (smooth transition)
                statusLED.setYellow();
            } else if (motorControl.getPWMDuty() > 0.1) {
                // Motor running - blue
                statusLED.setBlue();
            } else {
                // Motor idle - green
                statusLED.setGreen();
            }
            lastLEDUpdate = now;
        }

        // Yield to other tasks (10ms loop rate for smooth ramping)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    // ========== 步驟 1: 初始化 USB ==========
    USBSerial.begin();
    HID.begin();
    HID.onData(onHIDData);
    USB.begin();

    // ========== 步驟 1.5: 初始化馬達控制 ==========
    // Initialize motor settings manager
    if (!motorSettingsManager.begin()) {
        USBSerial.println("⚠️ Motor settings NVS initialization failed!");
    }

    // Load settings from NVS (or use defaults)
    motorSettingsManager.load();

    // Initialize status LED (must be before motor control for error indication)
    if (!statusLED.begin(48, motorSettingsManager.get().ledBrightness)) {
        USBSerial.println("⚠️ Status LED initialization failed!");
    } else {
        // Show yellow blinking during initialization
        statusLED.blinkYellow(200);
    }

    // Initialize motor control hardware
    if (!motorControl.begin(&motorSettingsManager.get())) {
        USBSerial.println("❌ Motor control initialization failed!");
        // Critical error - flash red LED fast
        statusLED.blinkRed(100);
        // Don't halt, but indicate error state
    } else {
        USBSerial.println("✅ Motor control initialized successfully");
    }

    // ========== 步驟 2: 創建 FreeRTOS 資源（必須在 BLE 初始化之前！）==========
    hidDataQueue = xQueueCreate(10, sizeof(HIDDataPacket));
    bleCommandQueue = xQueueCreate(10, sizeof(BLECommandPacket));  // BLE 命令佇列
    serialMutex = xSemaphoreCreateMutex();
    bufferMutex = xSemaphoreCreateMutex();
    hidSendMutex = xSemaphoreCreateMutex();
    bleNotifyQueue = xQueueCreate(32, sizeof(char*));

    // 檢查資源創建是否成功
    if (!hidDataQueue || !bleCommandQueue || !serialMutex || !bufferMutex || !hidSendMutex || !bleNotifyQueue) {
        USBSerial.println("❌ CRITICAL ERROR: FreeRTOS resource creation failed!");
        // Critical error - flash red LED fast and halt
        statusLED.blinkRed(100);
        statusLED.update();  // Update once to show the LED state
        // 如果資源創建失敗，進入無限迴圈（需要重啟）
        while (true) {
            statusLED.update();  // Keep LED blinking
            delay(10);
        }
    }

    // ========== 步驟 3: 創建回應物件 ==========
    cdc_response = new CDCResponse(USBSerial);
    hid_response = new HIDResponse(&HID);
    multi_response = new MultiChannelResponse(cdc_response, hid_response);

    // ========== 步驟 4: 等待 USB 連接（在 BLE 初始化之前）==========
    unsigned long start = millis();
    while (!USBSerial && (millis() - start < 5000)) {
        delay(100);
    }

    // ========== 步驟 5: 顯示歡迎訊息 ==========
    USBSerial.println("\n=================================");
    USBSerial.println("ESP32-S3 馬達控制系統");
    USBSerial.println("=================================");
    USBSerial.println("系統功能:");
    USBSerial.println("  ✅ USB CDC 序列埠控制台");
    USBSerial.println("  ✅ USB HID 自訂協定 (64 bytes)");
    USBSerial.println("  ✅ BLE GATT 無線介面");
    USBSerial.println("  ✅ WiFi Web 伺服器（AP/STA 模式）");
    USBSerial.println("  ✅ WebSocket 即時 RPM 監控");
    USBSerial.println("  ✅ REST API 馬達控制");
    USBSerial.println("  ✅ PWM 馬達控制 (MCPWM)");
    USBSerial.println("  ✅ 轉速計 RPM 量測");
    USBSerial.println("  ✅ FreeRTOS 多工架構");
    USBSerial.println("");
    USBSerial.println("硬體配置:");
    USBSerial.println("  GPIO 10: PWM 輸出");
    USBSerial.println("  GPIO 11: 轉速計輸入");
    USBSerial.println("  GPIO 12: 脈衝輸出");
    USBSerial.println("");
    USBSerial.printf("初始設定:\n");
    USBSerial.printf("  PWM 頻率: %d Hz\n", motorSettingsManager.get().frequency);
    USBSerial.printf("  PWM 占空比: %.1f%%\n", motorSettingsManager.get().duty);
    USBSerial.printf("  極對數: %d\n", motorSettingsManager.get().polePairs);
    USBSerial.println("");
    USBSerial.println("輸入 'HELP' 查看所有命令");
    USBSerial.println("=================================");

    // ========== 步驟 5.5: 初始化 SPIFFS 檔案系統 ==========
    USBSerial.println("");
    USBSerial.println("=== 初始化 SPIFFS 檔案系統 ===");

    if (!SPIFFS.begin(true)) {  // true = format if mount fails
        USBSerial.println("❌ SPIFFS mount failed!");
        USBSerial.println("  Web 介面將使用內建 HTML（備用模式）");
    } else {
        USBSerial.println("✅ SPIFFS mounted successfully");

        // List files in SPIFFS for debugging
        File root = SPIFFS.open("/");
        File file = root.openNextFile();
        if (file) {
            USBSerial.println("📁 SPIFFS files:");
            while (file) {
                USBSerial.printf("  - %s (%d bytes)\n", file.name(), file.size());
                file = root.openNextFile();
            }
        } else {
            USBSerial.println("  ⚠️ No files found in SPIFFS");
            USBSerial.println("  請使用 'pio run --target uploadfs' 上傳檔案");
        }
    }

    USBSerial.println("=================================");

    // ========== 步驟 6: 初始化 WiFi 和 Web 伺服器 ==========
    USBSerial.println("");
    USBSerial.println("=== 初始化 WiFi 和 Web 伺服器 ===");

    // Initialize WiFi settings
    if (!wifiSettingsManager.begin()) {
        USBSerial.println("⚠️ WiFi settings initialization failed, using defaults");
    }

    // Load WiFi settings from NVS
    wifiSettingsManager.load();
    const WiFiSettings& wifiSettings = wifiSettingsManager.get();

    // Initialize WiFi manager
    if (!wifiManager.begin(const_cast<WiFiSettings*>(&wifiSettings))) {
        USBSerial.println("❌ WiFi manager initialization failed!");
    } else {
        USBSerial.println("✅ WiFi manager initialized");
    }

    // Initialize web server
    if (!webServerManager.begin(
        const_cast<WiFiSettings*>(&wifiSettings),
        &motorControl,
        &motorSettingsManager,
        &wifiManager
    )) {
        USBSerial.println("❌ Web server initialization failed!");
    } else {
        USBSerial.println("✅ Web server initialized");
    }

    // Start WiFi if configured
    if (wifiSettings.mode != WiFiMode::OFF) {
        USBSerial.printf("🔧 啟動 WiFi 模式: ");
        switch (wifiSettings.mode) {
            case WiFiMode::AP:
                USBSerial.println("Access Point");
                break;
            case WiFiMode::STA:
                USBSerial.println("Station");
                break;
            case WiFiMode::AP_STA:
                USBSerial.println("AP + Station");
                break;
            default:
                USBSerial.println("Unknown");
                break;
        }

        if (wifiManager.start()) {
            USBSerial.println("✅ WiFi started successfully");

            // Start web server if WiFi is connected
            if (wifiManager.isConnected()) {
                if (webServerManager.start()) {
                    USBSerial.println("✅ Web server started successfully");
                    USBSerial.println("");
                    USBSerial.println("🌐 Web 介面資訊:");
                    USBSerial.printf("  URL: http://%s/\n", wifiManager.getIPAddress().c_str());
                    USBSerial.printf("  WebSocket: ws://%s/ws\n", wifiManager.getIPAddress().c_str());
                    USBSerial.println("  可透過網頁控制馬達並即時查看 RPM");
                } else {
                    USBSerial.println("⚠️ Web server failed to start");
                }
            }
        } else {
            USBSerial.println("⚠️ WiFi failed to start");
            USBSerial.println("  使用 'WIFI START' 命令手動啟動");
        }
    } else {
        USBSerial.println("ℹ️ WiFi 模式: OFF (未啟動)");
        USBSerial.println("  使用 'WIFI START' 命令啟動 WiFi");
    }

    USBSerial.println("=================================");

    // ========== 步驟 7: 初始化 BLE（現在 mutex 已準備好）==========
    USBSerial.println("[INFO] 正在初始化 BLE...");
    BLEDevice::init("ESP32_S3_Console");
    pBLEServer = BLEDevice::createServer();
    pBLEServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pBLEServer->createService(SERVICE_UUID);

    // TX Characteristic (用於發送資料到客戶端)
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // RX Characteristic (用於接收來自客戶端的資料)
    pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new MyRxCallbacks());

    pService->start();

    // 開始廣播
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();

    // 創建 BLE 回應物件
    ble_response = new BLEResponse(pTxCharacteristic);

    USBSerial.println("[INFO] BLE 初始化完成");
    USBSerial.println("\nBluetooth 資訊:");
    USBSerial.println("  BLE 裝置名稱: ESP32_S3_Console");
    USBSerial.println("=================================");
    USBSerial.print("\n> ");

    // 創建 FreeRTOS Tasks
    xTaskCreatePinnedToCore(
        hidTask,           // Task 函數
        "HID_Task",        // Task 名稱
        4096,              // Stack 大小
        NULL,              // 參數
        2,                 // 優先權（較高）
        NULL,              // Task handle
        1                  // Core 1
    );

    xTaskCreatePinnedToCore(
        cdcTask,           // Task 函數
        "CDC_Task",        // Task 名稱
        4096,              // Stack 大小
        NULL,              // 參數
        1,                 // 優先權（較低）
        NULL,              // Task handle
        1                  // Core 1
    );

    xTaskCreatePinnedToCore(
        bleTask,           // Task 函數
        "BLE_Task",        // Task 名稱
        4096,              // Stack 大小
        NULL,              // 參數
        1,                 // 優先權（與 CDC 相同）
        NULL,              // Task handle
        1                  // Core 1
    );

    xTaskCreatePinnedToCore(
        motorTask,         // Task 函數
        "Motor_Task",      // Task 名稱
        4096,              // Stack 大小
        NULL,              // 參數
        1,                 // 優先權（與 CDC 相同）
        NULL,              // Task handle
        1                  // Core 1
    );

    xTaskCreatePinnedToCore(
        wifiTask,          // Task 函數
        "WiFi_Task",       // Task 名稱
        8192,              // Stack 大小（較大，因為需要處理 WiFi 和 Web Server）
        NULL,              // 參數
        1,                 // 優先權（與 CDC 相同）
        NULL,              // Task handle
        1                  // Core 1
    );

    USBSerial.println("[INFO] FreeRTOS Tasks 已啟動");
    USBSerial.println("[INFO] - HID Task (優先權 2)");
    USBSerial.println("[INFO] - CDC Task (優先權 1)");
    USBSerial.println("[INFO] - BLE Task (優先權 1)");
    USBSerial.println("[INFO] - Motor Task (優先權 1)");
    USBSerial.println("[INFO] - WiFi Task (優先權 1)");

    // Set LED to green - system ready
    statusLED.setGreen();
    USBSerial.println("✅ System initialization complete - LED set to GREEN");
}

void loop() {
    // Update status LED (for blink timing)
    statusLED.update();

    // FreeRTOS Tasks 處理所有工作
    // loop() 保持空閒，讓 IDLE task 運行
    vTaskDelay(pdMS_TO_TICKS(50));  // Reduced to 50ms for smoother LED blinking
}
