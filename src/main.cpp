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

// USB CDC 實例（用於 console）
USBCDC USBSerial;

// 自訂 HID 實例（64 位元組，無 Report ID）
CustomHID64 HID;

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

// HID 命令緩衝區
String hid_command_buffer = "";

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

    // 等待 USB 連接（最多 5 秒）
    unsigned long start = millis();
    while (!USBSerial && (millis() - start < 5000)) {
        delay(100);
    }

    // 顯示歡迎訊息
    USBSerial.println("\n=================================");
    USBSerial.println("USB 複合裝置 - CDC + 自訂 HID (FreeRTOS)");
    USBSerial.println("=================================");
    USBSerial.println("裝置資訊:");
    USBSerial.println("  - CDC: 序列埠 console");
    USBSerial.println("  - HID: 64 位元組（無 Report ID）");
    USBSerial.println("  - 架構: FreeRTOS 多工處理");
    USBSerial.println("\n統一命令介面：");
    USBSerial.println("  所有命令必須以換行符 (\\n) 結尾");
    USBSerial.println("  CDC 和 HID 使用相同的命令集");
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

    USBSerial.println("[INFO] FreeRTOS Tasks 已啟動");
}

void loop() {
    // FreeRTOS Tasks 處理所有工作
    // loop() 保持空閒，讓 IDLE task 運行
    vTaskDelay(pdMS_TO_TICKS(1000));
}
