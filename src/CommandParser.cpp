#include "CommandParser.h"
#include "CustomHID.h"
#include "HIDProtocol.h"
#include "MotorControl.h"
#include "MotorSettings.h"
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

// Motor control external variables (from main.cpp)
extern MotorControl motorControl;
extern MotorSettingsManager motorSettingsManager;

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

    // RPM 讀取
    if (upper == "RPM") {
        handleRPM(response);
        return true;
    }

    // 馬達停止
    if (upper == "MOTOR STOP") {
        handleMotorStop(response);
        return true;
    }

    // 馬達狀態
    if (upper == "MOTOR STATUS") {
        handleMotorStatus(response);
        return true;
    }

    // 儲存設定
    if (upper == "SAVE") {
        handleSaveSettings(response);
        return true;
    }

    // 載入設定
    if (upper == "LOAD") {
        handleLoadSettings(response);
        return true;
    }

    // 重設設定
    if (upper == "RESET") {
        handleResetSettings(response);
        return true;
    }

    // SET 命令處理
    if (upper.startsWith("SET ")) {
        String params = upper.substring(4);
        params.trim();

        int spaceIndex = params.indexOf(' ');
        if (spaceIndex > 0) {
            String parameter = params.substring(0, spaceIndex);
            String value = params.substring(spaceIndex + 1);
            value.trim();

            // SET PWM_FREQ <Hz>
            if (parameter == "PWM_FREQ") {
                uint32_t freq = value.toInt();
                handleSetPWMFreq(response, freq);
                return true;
            }

            // SET PWM_DUTY <%>
            if (parameter == "PWM_DUTY") {
                float duty = value.toFloat();
                handleSetPWMDuty(response, duty);
                return true;
            }

            // SET POLE_PAIRS <num>
            if (parameter == "POLE_PAIRS") {
                uint8_t pairs = value.toInt();
                handleSetPolePairs(response, pairs);
                return true;
            }

            // SET MAX_FREQ <Hz>
            if (parameter == "MAX_FREQ") {
                uint32_t maxFreq = value.toInt();
                handleSetMaxFreq(response, maxFreq);
                return true;
            }

            // SET MAX_RPM <rpm>
            if (parameter == "MAX_RPM") {
                uint32_t maxRPM = value.toInt();
                handleSetMaxRPM(response, maxRPM);
                return true;
            }

            // SET LED_BRIGHTNESS <0-255>
            if (parameter == "LED_BRIGHTNESS") {
                uint8_t brightness = value.toInt();
                handleSetLEDBrightness(response, brightness);
                return true;
            }
        }

        response->println("❌ Invalid SET command format");
        response->println("Usage: SET <parameter> <value>");
        return false;
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

bool CommandParser::isSCPICommand(const String& cmd) {
    // 去除前後空白
    String trimmed = cmd;
    trimmed.trim();

    // 轉換為大寫以進行不區分大小寫的比較
    String upper = trimmed;
    upper.toUpperCase();

    // 檢查是否為 SCPI 命令（以 * 開頭或符合 SCPI 模式）
    if (upper.startsWith("*")) {
        return true;  // 所有以 * 開頭的都是 SCPI 命令，例如 *IDN?, *RST, *CLS
    }

    // 可以在此添加其他 SCPI 命令模式檢測
    // 例如: SYST:ERR?, MEAS:VOLT? 等

    return false;
}

void CommandParser::handleIDN(ICommandResponse* response) {
    response->println("HID_ESP32_S3");
}

void CommandParser::handleHelp(ICommandResponse* response) {
    response->println("");
    response->println("可用命令:");
    response->println("");
    response->println("一般命令:");
    response->println("  *IDN?         - 識別設備（SCPI 標準）");
    response->println("  HELP          - 顯示此說明");
    response->println("  INFO          - 顯示設備資訊");
    response->println("  STATUS        - 顯示系統狀態");
    response->println("");
    response->println("HID 測試:");
    response->println("  SEND          - 發送測試 HID IN 報告");
    response->println("  READ          - 讀取 HID OUT 緩衝區");
    response->println("  CLEAR         - 清除 HID OUT 緩衝區");
    response->println("");
    response->println("馬達控制:");
    response->println("  SET PWM_FREQ <Hz>    - 設定 PWM 頻率 (10-500000 Hz)");
    response->println("  SET PWM_DUTY <%>     - 設定 PWM 占空比 (0-100%)");
    response->println("  SET POLE_PAIRS <num> - 設定馬達極對數 (1-12)");
    response->println("  SET MAX_FREQ <Hz>    - 設定最大頻率限制");
    response->println("  SET MAX_RPM <rpm>    - 設定最大 RPM 限制");
    response->println("  SET LED_BRIGHTNESS <val> - 設定 LED 亮度 (0-255)");
    response->println("  RPM               - 顯示當前 RPM 讀數");
    response->println("  MOTOR STATUS      - 顯示馬達控制狀態");
    response->println("  MOTOR STOP        - 緊急停止（設定占空比為 0%）");
    response->println("");
    response->println("設定管理:");
    response->println("  SAVE          - 儲存設定到 NVS");
    response->println("  LOAD          - 從 NVS 載入設定");
    response->println("  RESET         - 重設為出廠預設值");
    response->println("");
    response->println("支援的介面:");
    response->println("  - USB CDC (序列埠)");
    response->println("  - USB HID (64位元組自訂協定)");
    response->println("  - BLE GATT (低功耗藍牙)");
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

// ==================== Motor Control Command Handlers ====================

void CommandParser::handleSetPWMFreq(ICommandResponse* response, uint32_t freq) {
    if (freq < MotorLimits::MIN_FREQUENCY || freq > MotorLimits::MAX_FREQUENCY) {
        response->printf("❌ 錯誤：頻率必須在 %d - %d Hz 之間\n",
                        MotorLimits::MIN_FREQUENCY, MotorLimits::MAX_FREQUENCY);
        return;
    }

    if (motorControl.setPWMFrequency(freq)) {
        response->printf("✅ PWM 頻率設定為: %d Hz\n", freq);
    } else {
        response->println("❌ 設定 PWM 頻率失敗");
    }
}

void CommandParser::handleSetPWMDuty(ICommandResponse* response, float duty) {
    if (duty < MotorLimits::MIN_DUTY || duty > MotorLimits::MAX_DUTY) {
        response->printf("❌ 錯誤：占空比必須在 %.0f - %.0f%% 之間\n",
                        MotorLimits::MIN_DUTY, MotorLimits::MAX_DUTY);
        return;
    }

    if (motorControl.setPWMDuty(duty)) {
        response->printf("✅ PWM 占空比設定為: %.1f%%\n", duty);
    } else {
        response->println("❌ 設定 PWM 占空比失敗");
    }
}

void CommandParser::handleSetPolePairs(ICommandResponse* response, uint8_t pairs) {
    if (pairs < MotorLimits::MIN_POLE_PAIRS || pairs > MotorLimits::MAX_POLE_PAIRS) {
        response->printf("❌ 錯誤：極對數必須在 %d - %d 之間\n",
                        MotorLimits::MIN_POLE_PAIRS, MotorLimits::MAX_POLE_PAIRS);
        return;
    }

    motorSettingsManager.get().polePairs = pairs;
    response->printf("✅ 馬達極對數設定為: %d\n", pairs);
    response->println("ℹ️ 使用 SAVE 命令儲存設定");
}

void CommandParser::handleSetMaxFreq(ICommandResponse* response, uint32_t maxFreq) {
    if (maxFreq < 1000 || maxFreq > MotorLimits::MAX_FREQUENCY) {
        response->printf("❌ 錯誤：最大頻率必須在 1000 - %d Hz 之間\n",
                        MotorLimits::MAX_FREQUENCY);
        return;
    }

    motorSettingsManager.get().maxFrequency = maxFreq;
    response->printf("✅ 最大頻率設定為: %d Hz\n", maxFreq);
    response->println("ℹ️ 使用 SAVE 命令儲存設定");
}

void CommandParser::handleSetMaxRPM(ICommandResponse* response, uint32_t maxRPM) {
    if (maxRPM < 1000 || maxRPM > 1000000) {
        response->println("❌ 錯誤：最大 RPM 必須在 1000 - 1000000 之間");
        return;
    }

    motorSettingsManager.get().maxSafeRPM = maxRPM;
    response->printf("✅ 最大 RPM 設定為: %d\n", maxRPM);
    response->println("ℹ️ 使用 SAVE 命令儲存設定");
}

void CommandParser::handleSetLEDBrightness(ICommandResponse* response, uint8_t brightness) {
    motorSettingsManager.get().ledBrightness = brightness;
    response->printf("✅ LED 亮度設定為: %d\n", brightness);
    response->println("ℹ️ 使用 SAVE 命令儲存設定");
}

void CommandParser::handleRPM(ICommandResponse* response) {
    response->println("");
    response->println("RPM 讀數:");
    response->printf("  當前 RPM: %.1f\n", motorControl.getCurrentRPM());
    response->printf("  輸入頻率: %.2f Hz\n", motorControl.getInputFrequency());
    response->printf("  極對數: %d\n", motorSettingsManager.get().polePairs);
    response->printf("  PWM 頻率: %d Hz\n", motorControl.getPWMFrequency());
    response->printf("  PWM 占空比: %.1f%%\n", motorControl.getPWMDuty());
    response->println("");
}

void CommandParser::handleMotorStatus(ICommandResponse* response) {
    const MotorSettings& settings = motorSettingsManager.get();

    response->println("");
    response->println("馬達控制狀態:");
    response->println("");

    // Initialization status
    response->printf("  初始化: %s\n", motorControl.isInitialized() ? "✅ 成功" : "❌ 失敗");
    response->printf("  轉速計: %s\n", motorControl.isCaptureInitialized() ? "✅ 就緒" : "❌ 未就緒");
    response->printf("  運行時間: %lu ms\n", motorControl.getUptime());
    response->println("");

    // PWM output status
    response->println("PWM 輸出:");
    response->printf("  頻率: %d Hz\n", motorControl.getPWMFrequency());
    response->printf("  占空比: %.1f%%\n", motorControl.getPWMDuty());
    response->printf("  最大頻率限制: %d Hz\n", settings.maxFrequency);
    response->println("");

    // Tachometer status
    response->println("轉速計:");
    response->printf("  當前 RPM: %.1f\n", motorControl.getCurrentRPM());
    response->printf("  輸入頻率: %.2f Hz\n", motorControl.getInputFrequency());
    response->printf("  極對數: %d\n", settings.polePairs);
    response->printf("  最大 RPM 限制: %d\n", settings.maxSafeRPM);
    response->printf("  更新間隔: %d ms\n", settings.rpmUpdateRate);
    response->println("");

    // Safety status
    response->println("安全檢查:");
    bool safetyOK = motorControl.checkSafety();
    response->printf("  狀態: %s\n", safetyOK ? "✅ 正常" : "⚠️ 警告");
    if (motorControl.getCurrentRPM() > settings.maxSafeRPM) {
        response->println("  ⚠️ 超速偵測");
    }
    if (motorControl.getPWMDuty() > 10.0f && motorControl.getCurrentRPM() < 100.0f) {
        response->println("  ⚠️ 可能停轉");
    }
    response->println("");
}

void CommandParser::handleMotorStop(ICommandResponse* response) {
    motorControl.emergencyStop();
    response->println("⛔ 緊急停止已啟動 - 占空比設為 0%");
}

void CommandParser::handleSaveSettings(ICommandResponse* response) {
    if (motorSettingsManager.save()) {
        response->println("✅ 設定已儲存到 NVS");
    } else {
        response->println("❌ 儲存設定失敗");
    }
}

void CommandParser::handleLoadSettings(ICommandResponse* response) {
    if (motorSettingsManager.load()) {
        const MotorSettings& settings = motorSettingsManager.get();
        response->println("✅ 設定已從 NVS 載入");
        response->printf("  PWM 頻率: %d Hz\n", settings.frequency);
        response->printf("  PWM 占空比: %.1f%%\n", settings.duty);
        response->printf("  極對數: %d\n", settings.polePairs);
        response->printf("  最大頻率: %d Hz\n", settings.maxFrequency);
        response->printf("  最大 RPM: %d\n", settings.maxSafeRPM);

        // Apply loaded settings to motor control
        motorControl.setPWMFrequency(settings.frequency);
        motorControl.setPWMDuty(settings.duty);
    } else {
        response->println("❌ 載入設定失敗");
    }
}

void CommandParser::handleResetSettings(ICommandResponse* response) {
    motorSettingsManager.reset();
    motorSettingsManager.save();
    response->println("✅ 設定已重設為出廠預設值");
    response->printf("  PWM 頻率: %d Hz\n", MotorDefaults::FREQUENCY);
    response->printf("  PWM 占空比: %.0f%%\n", MotorDefaults::DUTY);
    response->printf("  極對數: %d\n", MotorDefaults::POLE_PAIRS);

    // Apply default settings
    motorControl.setPWMFrequency(MotorDefaults::FREQUENCY);
    motorControl.setPWMDuty(MotorDefaults::DUTY);
}

// ==================== HID Response Implementation ====================

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
        delay(50);  // 增加延遲確保 BLE stack 處理完成
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
        delay(50);  // 增加延遲確保 BLE stack 處理完成
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
