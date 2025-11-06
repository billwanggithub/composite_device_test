#include "CommandParser.h"
#include "CustomHID.h"
#include "HIDProtocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <cstring>

// 外部變數（從 main.cpp）
extern CustomHID64 HID;
extern uint8_t hid_out_buffer[64];
extern bool hid_data_ready;
extern SemaphoreHandle_t bufferMutex;
extern SemaphoreHandle_t hidSendMutex;

CommandParser::CommandParser() {
}

bool CommandParser::processCommand(const String& cmd, ICommandResponse* response, CommandSource source) {
    // 去除前後空白
    String trimmed = cmd;
    trimmed.trim();

    // 空命令
    if (trimmed.length() == 0) {
        return false;
    }

    // 轉換為大寫以進行不區分大小寫的比較
    String upper = trimmed;
    upper.toUpperCase();

    // SCPI 標準識別命令
    if (upper == "*IDN?") {
        handleIDN(response);
        return true;
    }

    // 說明命令
    if (upper == "HELP" || upper == "?") {
        handleHelp(response);
        return true;
    }

    // 資訊命令
    if (upper == "INFO") {
        handleInfo(response);
        return true;
    }

    // 狀態命令
    if (upper == "STATUS") {
        handleStatus(response);
        return true;
    }

    // 發送測試 HID 報告
    if (upper == "SEND") {
        handleSend(response);
        return true;
    }

    // 讀取 HID OUT 緩衝區
    if (upper == "READ") {
        handleRead(response);
        return true;
    }

    // 清除 HID OUT 緩衝區
    if (upper == "CLEAR") {
        handleClear(response);
        return true;
    }

    // 未知命令
    response->print("未知命令: ");
    response->println(trimmed.c_str());
    response->println("輸入 'HELP' 查看可用命令");
    return false;
}

bool CommandParser::feedChar(char c, String& buffer, ICommandResponse* response, CommandSource source) {
    // 換行符表示命令結束
    if (c == '\n' || c == '\r') {
        if (buffer.length() > 0) {
            // 處理命令
            bool result = processCommand(buffer, response, source);
            buffer = "";  // 清空緩衝區
            return result;
        }
        return false;
    }

    // 退格鍵
    if (c == '\b' || c == 127) {
        if (buffer.length() > 0) {
            buffer.remove(buffer.length() - 1);
        }
        return false;
    }

    // 可列印字元
    if (c >= 32 && c < 127) {
        buffer += c;
        // 不回顯字元（用戶不希望看到輸入的命令）
        return false;
    }

    return false;
}

void CommandParser::handleIDN(ICommandResponse* response) {
    response->println("HID_ESP32_S3");
}

void CommandParser::handleHelp(ICommandResponse* response) {
    response->println("");
    response->println("可用命令:");
    response->println("  *IDN?   - 識別設備（SCPI 標準）");
    response->println("  HELP    - 顯示此說明");
    response->println("  INFO    - 顯示設備資訊");
    response->println("  STATUS  - 顯示系統狀態");
    response->println("  SEND    - 發送測試 HID IN 報告");
    response->println("  READ    - 讀取 HID OUT 緩衝區");
    response->println("  CLEAR   - 清除 HID OUT 緩衝區");
    response->println("");
    response->println("支援的介面:");
    response->println("  - USB CDC (序列埠)");
    response->println("  - USB HID (64位元組自訂協定)");
    response->println("  - BLE GATT (低功耗藍牙)");
    // Classic Bluetooth Serial (SPP) is not available on ESP32-S3; use BLE GATT instead
    response->println("");
    response->println("所有命令必須以換行符結尾");
}

void CommandParser::handleInfo(ICommandResponse* response) {
    response->println("");
    response->println("=== ESP32-S3 裝置資訊 ===");
    response->println("");
    response->println("硬體規格:");
    response->println("  型號: ESP32-S3-DevKitC-1 N16R8");
    response->println("  晶片: ESP32-S3");
    response->printf("  Flash 大小: %u bytes (%.2f MB)\n",
                     ESP.getFlashChipSize(),
                     ESP.getFlashChipSize() / 1024.0 / 1024.0);
    response->printf("  PSRAM 總量: %u bytes (%.2f MB)\n",
                     ESP.getPsramSize(),
                     ESP.getPsramSize() / 1024.0 / 1024.0);
    response->printf("  PSRAM 可用: %u bytes (%.2f MB)\n",
                     ESP.getFreePsram(),
                     ESP.getFreePsram() / 1024.0 / 1024.0);
    response->println("");
    response->println("記憶體狀態:");
    response->printf("  Heap 總量: %u bytes (%.2f KB)\n",
                     ESP.getHeapSize(),
                     ESP.getHeapSize() / 1024.0);
    response->printf("  Heap 可用: %u bytes (%.2f KB)\n",
                     ESP.getFreeHeap(),
                     ESP.getFreeHeap() / 1024.0);
    response->println("");
    response->println("通訊介面:");
    response->println("  USB CDC: 已啟用");
    response->println("  USB HID: 64 位元組（無 Report ID）");
    response->println("  BLE GATT: 已啟用");
    response->println("  BT Serial: 未啟用 (ESP32-S3 不支援 Classic SPP)");
}

void CommandParser::handleStatus(ICommandResponse* response) {
    response->println("");
    response->println("系統狀態:");
    response->printf("  運行時間: %lu ms\n", millis());
    response->printf("  自由記憶體: %d bytes\n", ESP.getFreeHeap());
    response->printf("  HID OUT 已接收: %s\n", hid_data_ready ? "是" : "否");
}

void CommandParser::handleSend(ICommandResponse* response) {
    // 填充測試資料（0x00 到 0x3F）
    uint8_t test_data[64];
    for (int i = 0; i < 64; i++) {
        test_data[i] = i;
    }

    // 傳送 HID IN 報告（原始資料，不加命令協定 header）
    bool sent = false;
    if (xSemaphoreTake(hidSendMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        sent = HID.send(test_data, 64);
        xSemaphoreGive(hidSendMutex);
    }

    if (sent) {
        response->println("HID IN 報告已傳送 (64 位元組)");
        response->print("資料: ");
        for (int i = 0; i < 16; i++) {
            response->printf("%02X ", test_data[i]);
        }
        response->println("...");
    } else {
        response->println("傳送失敗！");
    }
}

void CommandParser::handleRead(ICommandResponse* response) {
    // 取得 mutex 保護緩衝區存取
    if (bufferMutex && xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(100))) {
        if (hid_data_ready) {
            response->println("");
            response->println("HID OUT 緩衝區內容:");
            for (int i = 0; i < 64; i++) {
                if (i % 16 == 0) {
                    response->printf("\n%04X: ", i);
                }
                response->printf("%02X ", hid_out_buffer[i]);
            }
            response->println("");
        } else {
            response->println("尚未接收到 HID OUT 資料");
        }
        xSemaphoreGive(bufferMutex);
    } else {
        response->println("錯誤：無法存取緩衝區");
    }
}

void CommandParser::handleClear(ICommandResponse* response) {
    // 取得 mutex 保護緩衝區存取
    if (bufferMutex && xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(100))) {
        memset(hid_out_buffer, 0, 64);
        hid_data_ready = false;
        xSemaphoreGive(bufferMutex);
        response->println("HID OUT 緩衝區已清除");
    } else {
        response->println("錯誤：無法存取緩衝區");
    }
}

// HIDResponse 實作
void HIDResponse::sendString(const char* str) {
    size_t len = strlen(str);
    size_t offset = 0;

    // 分割成最多 61-byte 的包（因為需要 3-byte header: [0xA1][length][0x00]）
    while (offset < len) {
        uint8_t encoded_buffer[64] = {0};
        size_t chunk_size = (len - offset) > 61 ? 61 : (len - offset);

        // 使用 HIDProtocol 編碼（加上 3-byte header）
        HIDProtocol::encodeResponse(encoded_buffer, (const uint8_t*)(str + offset), chunk_size);

        // 使用 mutex 保護 HID.send()
        if (xSemaphoreTake(hidSendMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            HID.send(encoded_buffer, 64);
            xSemaphoreGive(hidSendMutex);
        }

        offset += chunk_size;

        // 短暫延遲確保主機接收
        delay(10);
    }
}

// BLEResponse 實作
// If no BLE client is connected, queue notifications so they will be flushed
// when a client connects.
extern bool bleDeviceConnected;
extern QueueHandle_t bleNotifyQueue;

void BLEResponse::print(const char* str) {
    if (!_characteristic) return;
    BLECharacteristic* pCharacteristic = static_cast<BLECharacteristic*>(_characteristic);
    size_t len = strlen(str);

    if (bleDeviceConnected) {
        pCharacteristic->setValue((uint8_t*)str, len);
        pCharacteristic->notify();
        delay(10);  // 確保 BLE 封包傳送
        return;
    }

    // Not connected: enqueue a copy of the string (heap-allocated)
    if (bleNotifyQueue) {
        char* copy = (char*)strdup(str);
        if (copy) {
            BaseType_t ok = xQueueSend(bleNotifyQueue, &copy, 0);
            if (ok != pdTRUE) {
                // queue full or failed; drop message
                free(copy);
            }
        }
    }
}

void BLEResponse::println(const char* str) {
    // build a newline-terminated copy and reuse print/path
    size_t len = strlen(str);
    char* buf = (char*)malloc(len + 2);
    if (!buf) return;
    memcpy(buf, str, len);
    buf[len] = '\n';
    buf[len+1] = '\0';

    // If connected, send immediately; otherwise enqueue
    if (bleDeviceConnected && _characteristic) {
        BLECharacteristic* pCharacteristic = static_cast<BLECharacteristic*>(_characteristic);
        pCharacteristic->setValue((uint8_t*)buf, len+1);
        pCharacteristic->notify();
        delay(10);
        free(buf);
        return;
    }

    if (bleNotifyQueue) {
        char* copy = (char*)buf; // take ownership
        BaseType_t ok = xQueueSend(bleNotifyQueue, &copy, 0);
        if (ok != pdTRUE) {
            free(copy);
        }
    } else {
        free(buf);
    }
}

void BLEResponse::printf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    print(buffer);
}

// BTSerialResponse 實作
// Classic BT SPP support is intentionally omitted for ESP32-S3 builds.
