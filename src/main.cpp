#include <Arduino.h>
#include "USB.h"
#include "USBCDC.h"
#include "CustomHID.h"
#include "CommandParser.h"
#include "HIDProtocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "BluetoothSerial.h"

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

// Bluetooth Serial 實例
BluetoothSerial SerialBT;

// HID 資料結構
typedef struct {
    uint8_t data[64];
    uint16_t len;
    uint8_t report_id;
    uint16_t raw_len;
} HIDDataPacket;

// FreeRTOS 資源
QueueHandle_t hidDataQueue = nullptr;      // HID 資料佇列
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
BTSerialResponse* bt_serial_response = nullptr;

// HID 命令緩衝區
String hid_command_buffer = "";

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        bleDeviceConnected = true;
        USBSerial.println("[BLE] 客戶端已連接");
    }

    void onDisconnect(BLEServer* pServer) {
        bleDeviceConnected = false;
        USBSerial.println("[BLE] 客戶端已斷開");
    }
};

// BLE RX Characteristic Callbacks (接收來自客戶端的命令)
class MyRxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0) {
            String command = String(rxValue.c_str());
            command.trim();

            if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
                USBSerial.printf("\n[BLE CMD] %s\n", command.c_str());
                xSemaphoreGive(serialMutex);
            }

            // 處理 BLE 命令（同時輸出到 BLE 和 CDC）
            MultiChannelResponse bleMultiResponse(ble_response, cdc_response);
            parser.processCommand(command, &bleMultiResponse, CMD_SOURCE_BLE);
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

                // 執行命令（使用多通道回應，同時輸出到 HID 和 CDC）
                String cmd_str(command_buffer);
                parser.processCommand(cmd_str, multi_response, CMD_SOURCE_HID);

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
        // 處理 CDC 輸入
        while (USBSerial.available()) {
            char c = USBSerial.read();

            // 取得 mutex 保護 USBSerial 輸出
            if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
                // 使用命令解析器處理字元（CDC 命令只輸出到 CDC）
                if (parser.feedChar(c, cdc_command_buffer, cdc_response, CMD_SOURCE_CDC)) {
                    // 命令已處理，顯示提示符
                    USBSerial.print("\n> ");
                }
                xSemaphoreGive(serialMutex);
            }
        }

        // 短暫延遲避免佔用 CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Bluetooth Serial 處理 Task
void btSerialTask(void* parameter) {
    String bt_command_buffer = "";

    while (true) {
        // 處理 Bluetooth Serial 輸入
        while (SerialBT.available()) {
            char c = SerialBT.read();

            // 處理字元並執行命令（BT Serial 命令同時輸出到 BT Serial 和 CDC）
            MultiChannelResponse btMultiResponse(bt_serial_response, cdc_response);

            // 儲存當前命令用於除錯顯示
            String current_cmd = bt_command_buffer;

            if (parser.feedChar(c, bt_command_buffer, &btMultiResponse, CMD_SOURCE_BT_SERIAL)) {
                // 命令已處理，顯示提示符
                SerialBT.print("\n> ");

                // 同時在 CDC 上顯示除錯資訊
                if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
                    USBSerial.printf("\n[BT Serial CMD] %s\n", current_cmd.c_str());
                    USBSerial.print("> ");
                    xSemaphoreGive(serialMutex);
                }
            }
        }

        // 短暫延遲避免佔用 CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    // 初始化 USB CDC
    USBSerial.begin();

    // 初始化自訂 HID（64 位元組，無 Report ID）
    HID.begin();
    HID.onData(onHIDData);

    // 啟動 USB
    USB.begin();

    // 創建回應物件
    cdc_response = new CDCResponse(USBSerial);
    hid_response = new HIDResponse(&HID);
    multi_response = new MultiChannelResponse(cdc_response, hid_response);

    // 初始化 BLE
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

    // 初始化 Bluetooth Serial
    if (!SerialBT.begin("ESP32_S3_BT_Console")) {
        USBSerial.println("[ERROR] Bluetooth Serial 初始化失敗！");
    } else {
        USBSerial.println("[INFO] Bluetooth Serial 已啟動");
    }

    // 創建 Bluetooth Serial 回應物件
    bt_serial_response = new BTSerialResponse(&SerialBT);

    // 等待 USB 連接（最多 5 秒）
    unsigned long start = millis();
    while (!USBSerial && (millis() - start < 5000)) {
        delay(100);
    }

    // 顯示歡迎訊息
    USBSerial.println("\n=================================");
    USBSerial.println("多介面複合裝置 (FreeRTOS)");
    USBSerial.println("=================================");
    USBSerial.println("裝置資訊:");
    USBSerial.println("  - USB CDC: 序列埠 console");
    USBSerial.println("  - USB HID: 64 位元組（無 Report ID）");
    USBSerial.println("  - BLE GATT: 命令介面");
    USBSerial.println("  - Bluetooth Serial: SPP console");
    USBSerial.println("  - 架構: FreeRTOS 多工處理");
    USBSerial.println("\n統一命令介面：");
    USBSerial.println("  所有介面使用相同的命令集");
    USBSerial.println("  所有命令必須以換行符 (\\n) 結尾");
    USBSerial.println("\nBluetooth 資訊:");
    USBSerial.println("  BLE 裝置名稱: ESP32_S3_Console");
    USBSerial.println("  BT Serial 名稱: ESP32_S3_BT_Console");
    USBSerial.println("\n輸入 'HELP' 查看可用命令");
    USBSerial.println("=================================");
    USBSerial.print("\n> ");

    // 創建 FreeRTOS 資源
    hidDataQueue = xQueueCreate(10, sizeof(HIDDataPacket));  // 10 個封包的佇列
    serialMutex = xSemaphoreCreateMutex();
    bufferMutex = xSemaphoreCreateMutex();
    hidSendMutex = xSemaphoreCreateMutex();  // HID 發送互斥鎖

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
        btSerialTask,      // Task 函數
        "BT_Serial_Task",  // Task 名稱
        4096,              // Stack 大小
        NULL,              // 參數
        1,                 // 優先權（與 CDC 相同）
        NULL,              // Task handle
        1                  // Core 1
    );

    USBSerial.println("[INFO] FreeRTOS Tasks 已啟動");
    USBSerial.println("[INFO] - HID Task (優先權 2)");
    USBSerial.println("[INFO] - CDC Task (優先權 1)");
    USBSerial.println("[INFO] - BT Serial Task (優先權 1)");
    USBSerial.println("[INFO] - BLE 透過 callback 處理");
}

void loop() {
    // FreeRTOS Tasks 處理所有工作
    // loop() 保持空閒，讓 IDLE task 運行
    vTaskDelay(pdMS_TO_TICKS(1000));
}
