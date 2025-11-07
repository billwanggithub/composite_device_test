#include "CommandParser.h"
#include "CustomHID.h"
#include "HIDProtocol.h"
#include "MotorControl.h"
#include "MotorSettings.h"
#include "StatusLED.h"
#include "WiFiManager.h"
#include "WebServer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
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
extern StatusLED statusLED;

// WiFi and Web Server external variables (from main.cpp)
extern WiFiManager wifiManager;
extern WiFiSettingsManager wifiSettingsManager;
extern WebServerManager webServerManager;

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

    // 清除緊急停止狀態
    if (upper == "CLEAR ERROR" || upper == "CLEAR_ERROR" || upper == "RESUME") {
        motorControl.clearEmergencyStop();
        response->println("✅ 錯誤已清除 - 系統已恢復正常");
        response->println("Emergency error cleared - System resumed");

        // Notify web clients that error is cleared
        if (webServerManager.isRunning()) {
            webServerManager.broadcastStatus();
        }
        return true;
    }

    // RPM 讀取
    if (upper == "RPM") {
        handleRPM(response);
        return true;
    }

    // 濾波器狀態
    if (upper == "FILTER STATUS") {
        handleFilterStatus(response);
        return true;
    }

    // RAMP 命令 - 格式: RAMP PWM_FREQ <Hz> <ms> 或 RAMP PWM_DUTY <%> <ms>
    if (upper.startsWith("RAMP ")) {
        String params = upper.substring(5);  // Remove "RAMP "
        params.trim();

        // Parse: PARAMETER VALUE TIME
        int firstSpace = params.indexOf(' ');
        if (firstSpace == -1) {
            response->println("❌ 錯誤：格式應為 RAMP <parameter> <value> <time_ms>");
            return true;
        }

        String parameter = params.substring(0, firstSpace);
        parameter.trim();

        String rest = params.substring(firstSpace + 1);
        rest.trim();

        int secondSpace = rest.indexOf(' ');
        if (secondSpace == -1) {
            response->println("❌ 錯誤：格式應為 RAMP <parameter> <value> <time_ms>");
            return true;
        }

        String value = rest.substring(0, secondSpace);
        value.trim();

        String timeStr = rest.substring(secondSpace + 1);
        timeStr.trim();
        uint32_t rampTimeMs = timeStr.toInt();

        if (parameter == "PWM_FREQ") {
            uint32_t freq = value.toInt();
            handleSetPWMFreqRamped(response, freq, rampTimeMs);
            return true;
        }

        if (parameter == "PWM_DUTY") {
            float duty = value.toFloat();
            handleSetPWMDutyRamped(response, duty, rampTimeMs);
            return true;
        }

        response->println("❌ 錯誤：不支援的 RAMP 參數（支援: PWM_FREQ, PWM_DUTY）");
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

            // SET RPM_FILTER_SIZE <size>
            if (parameter == "RPM_FILTER_SIZE") {
                uint8_t size = value.toInt();
                handleSetRPMFilterSize(response, size);
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

    // WiFi 連線命令: WIFI <ssid> <password>
    if (upper.startsWith("WIFI ") && !upper.startsWith("WIFI STATUS") &&
        !upper.startsWith("WIFI START") && !upper.startsWith("WIFI STOP") &&
        !upper.startsWith("WIFI SCAN")) {
        handleWiFiConnect(trimmed, response);
        return true;
    }

    // IP 地址顯示
    if (upper == "IP") {
        handleIPAddress(response);
        return true;
    }

    // WiFi 狀態
    if (upper == "WIFI STATUS") {
        handleWiFiStatus(response);
        return true;
    }

    // WiFi 啟動
    if (upper == "WIFI START") {
        handleWiFiStart(response);
        return true;
    }

    // WiFi 停止
    if (upper == "WIFI STOP") {
        handleWiFiStop(response);
        return true;
    }

    // WiFi 掃描
    if (upper == "WIFI SCAN") {
        handleWiFiScan(response);
        return true;
    }

    // Web 伺服器狀態
    if (upper == "WEB STATUS") {
        handleWebStatus(response);
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
    response->println("  CLEAR ERROR (or RESUME) - 清除緊急停止狀態");
    response->println("");
    response->println("進階功能 (Priority 3):");
    response->println("  RAMP PWM_FREQ <Hz> <ms>  - 漸變 PWM 頻率");
    response->println("  RAMP PWM_DUTY <%> <ms>   - 漸變 PWM 占空比");
    response->println("  SET RPM_FILTER_SIZE <n>  - 設定 RPM 濾波器大小 (1-20)");
    response->println("  FILTER STATUS           - 顯示濾波器狀態");
    response->println("");
    response->println("設定管理:");
    response->println("  SAVE          - 儲存設定到 NVS");
    response->println("  LOAD          - 從 NVS 載入設定");
    response->println("  RESET         - 重設為出廠預設值");
    response->println("");
    response->println("WiFi & Web 伺服器:");
    response->println("  WIFI <ssid> <password> - 連接到 WiFi 網路");
    response->println("  IP            - 顯示 IP 位址資訊");
    response->println("  WIFI STATUS   - 顯示 WiFi 連線狀態");
    response->println("  WIFI START    - 啟動 WiFi");
    response->println("  WIFI STOP     - 停止 WiFi");
    response->println("  WIFI SCAN     - 掃描可用網路");
    response->println("  WEB STATUS    - 顯示 Web 伺服器狀態");
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
    // Check against absolute hardware limits
    if (freq < MotorLimits::MIN_FREQUENCY || freq > MotorLimits::MAX_FREQUENCY) {
        response->printf("❌ 錯誤：頻率必須在 %d - %d Hz 之間 (硬體限制)\n",
                        MotorLimits::MIN_FREQUENCY, MotorLimits::MAX_FREQUENCY);
        return;
    }

    // Check against user-configurable safety limit
    if (freq > motorSettingsManager.get().maxFrequency) {
        response->printf("❌ 錯誤：頻率 %d Hz 超過安全限制 %d Hz\n",
                        freq, motorSettingsManager.get().maxFrequency);
        response->printf("   使用 'SET MAX_FREQ %d' 來提高限制\n", freq);
        return;
    }

    if (motorControl.setPWMFrequency(freq)) {
        response->printf("✅ PWM 頻率設定為: %d Hz\n", freq);

        // Notify web clients about the change
        if (webServerManager.isRunning()) {
            webServerManager.broadcastStatus();
        }
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

        // Notify web clients about the change
        if (webServerManager.isRunning()) {
            webServerManager.broadcastStatus();
        }
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
    motorControl.setPolePairs(pairs);  // Apply to motor control immediately
    response->printf("✅ 馬達極對數設定為: %d\n", pairs);
    response->println("ℹ️ 使用 SAVE 命令儲存設定");

    // Notify web clients about the change
    if (webServerManager.isRunning()) {
        webServerManager.broadcastStatus();
    }
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

    // Notify web clients about the change
    if (webServerManager.isRunning()) {
        webServerManager.broadcastStatus();
    }
}

void CommandParser::handleSetMaxRPM(ICommandResponse* response, uint32_t maxRPM) {
    if (maxRPM < 1000 || maxRPM > 1000000) {
        response->println("❌ 錯誤：最大 RPM 必須在 1000 - 1000000 之間");
        return;
    }

    motorSettingsManager.get().maxSafeRPM = maxRPM;
    response->printf("✅ 最大 RPM 設定為: %d\n", maxRPM);
    response->println("ℹ️ 使用 SAVE 命令儲存設定");

    // Notify web clients about the change
    if (webServerManager.isRunning()) {
        webServerManager.broadcastStatus();
    }
}

void CommandParser::handleSetLEDBrightness(ICommandResponse* response, uint8_t brightness) {
    motorSettingsManager.get().ledBrightness = brightness;

    // Apply brightness to LED hardware immediately
    if (statusLED.isInitialized()) {
        statusLED.setBrightness(brightness);
        response->printf("✅ LED 亮度設定為: %d (已立即套用)\n", brightness);
    } else {
        response->printf("✅ LED 亮度設定為: %d (LED 未初始化)\n", brightness);
    }

    response->println("ℹ️ 使用 SAVE 命令儲存設定");

    // Notify web clients about the change
    if (webServerManager.isRunning()) {
        webServerManager.broadcastStatus();
    }
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

    // Advanced features status (Priority 3)
    response->println("進階功能:");
    response->printf("  RPM 濾波器大小: %d 個樣本\n", motorControl.getRPMFilterSize());
    response->printf("  原始 RPM: %.0f RPM\n", motorControl.getRawRPM());
    response->printf("  濾波後 RPM: %.0f RPM\n", motorControl.getCurrentRPM());
    response->printf("  PWM 漸變: %s\n", motorControl.isRamping() ? "🔄 進行中" : "✅ 閒置");
    response->printf("  看門狗: %s\n", motorControl.checkWatchdog() ? "✅ 正常" : "⚠️ 逾時");
    response->println("");

    // Safety status
    response->println("安全檢查:");
    bool safetyOK = motorControl.checkSafety();
    bool watchdogOK = motorControl.checkWatchdog();
    response->printf("  狀態: %s\n", (safetyOK && watchdogOK) ? "✅ 正常" : "⚠️ 警告");
    if (motorControl.getCurrentRPM() > settings.maxSafeRPM) {
        response->println("  ⚠️ 超速偵測");
    }
    if (motorControl.getPWMDuty() > 10.0f && motorControl.getCurrentRPM() < 100.0f) {
        response->println("  ⚠️ 可能停轉");
    }
    if (!watchdogOK) {
        response->println("  ⚠️ 看門狗逾時");
    }
    response->println("");
}

void CommandParser::handleMotorStop(ICommandResponse* response) {
    motorControl.emergencyStop();  // This captures the trigger RPM internally

    float triggerRPM = motorControl.getEmergencyStopTriggerRPM();
    uint32_t maxSafeRPM = motorSettingsManager.get().maxSafeRPM;

    response->println("⛔ 緊急停止已啟動 - 占空比設為 0%");
    response->printf("   觸發 RPM: %.1f / 最大安全 RPM: %u\n", triggerRPM, maxSafeRPM);

    // Notify web clients about emergency stop
    if (webServerManager.isRunning()) {
        webServerManager.broadcastStatus();
    }
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
        motorControl.setPolePairs(settings.polePairs);
        statusLED.setBrightness(settings.ledBrightness);

        // Notify web clients about the changes
        if (webServerManager.isRunning()) {
            webServerManager.broadcastStatus();
        }
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
    motorControl.setPolePairs(MotorDefaults::POLE_PAIRS);
    statusLED.setBrightness(MotorDefaults::LED_BRIGHTNESS);

    // Notify web clients about the changes
    if (webServerManager.isRunning()) {
        webServerManager.broadcastStatus();
    }
}

// ==================== Advanced Features (Priority 3) ====================

void CommandParser::handleSetPWMFreqRamped(ICommandResponse* response, uint32_t freq, uint32_t rampTimeMs) {
    if (freq < MotorLimits::MIN_FREQUENCY || freq > MotorLimits::MAX_FREQUENCY) {
        response->printf("❌ 錯誤：頻率必須在 %d - %d Hz 之間\n",
                        MotorLimits::MIN_FREQUENCY, MotorLimits::MAX_FREQUENCY);
        return;
    }

    if (rampTimeMs == 0) {
        response->println("⚠️ 漸變時間為 0，將立即設定");
        handleSetPWMFreq(response, freq);
        return;
    }

    if (motorControl.setPWMFrequencyRamped(freq, rampTimeMs)) {
        response->printf("✅ 開始頻率漸變: %d Hz → %d Hz (耗時 %d ms)\n",
                        motorControl.getPWMFrequency(), freq, rampTimeMs);

        // Notify web clients - they will see gradual change via periodic updates
        if (webServerManager.isRunning()) {
            webServerManager.broadcastStatus();
        }
    } else {
        response->println("❌ 啟動頻率漸變失敗");
    }
}

void CommandParser::handleSetPWMDutyRamped(ICommandResponse* response, float duty, uint32_t rampTimeMs) {
    if (duty < MotorLimits::MIN_DUTY || duty > MotorLimits::MAX_DUTY) {
        response->printf("❌ 錯誤：占空比必須在 %.0f - %.0f%% 之間\n",
                        MotorLimits::MIN_DUTY, MotorLimits::MAX_DUTY);
        return;
    }

    if (rampTimeMs == 0) {
        response->println("⚠️ 漸變時間為 0，將立即設定");
        handleSetPWMDuty(response, duty);
        return;
    }

    if (motorControl.setPWMDutyRamped(duty, rampTimeMs)) {
        response->printf("✅ 開始占空比漸變: %.1f%% → %.1f%% (耗時 %d ms)\n",
                        motorControl.getPWMDuty(), duty, rampTimeMs);

        // Notify web clients - they will see gradual change via periodic updates
        if (webServerManager.isRunning()) {
            webServerManager.broadcastStatus();
        }
    } else {
        response->println("❌ 啟動占空比漸變失敗");
    }
}

void CommandParser::handleSetRPMFilterSize(ICommandResponse* response, uint8_t size) {
    if (size < 1 || size > 20) {
        response->println("❌ 錯誤：濾波器大小必須在 1 - 20 之間");
        return;
    }

    motorControl.setRPMFilterSize(size);
    response->printf("✅ RPM 濾波器大小已設定為: %d 個樣本\n", size);
}

void CommandParser::handleFilterStatus(ICommandResponse* response) {
    response->println("=== RPM 濾波器狀態 ===");
    response->printf("濾波器大小: %d 個樣本\n", motorControl.getRPMFilterSize());
    response->printf("原始 RPM: %.0f RPM\n", motorControl.getRawRPM());
    response->printf("濾波後 RPM: %.0f RPM\n", motorControl.getCurrentRPM());

    float difference = motorControl.getCurrentRPM() - motorControl.getRawRPM();
    response->printf("濾波差異: %.1f RPM\n", difference);

    if (motorControl.isRamping()) {
        response->println("");
        response->println("⚙️ PWM 漸變進行中...");
        response->printf("  當前頻率: %d Hz\n", motorControl.getPWMFrequency());
        response->printf("  當前占空比: %.1f%%\n", motorControl.getPWMDuty());
    }

    response->println("");
}

// ==================== WiFi and Web Server Commands ====================

void CommandParser::handleWiFiStatus(ICommandResponse* response) {
    response->println("=== WiFi 狀態 ===");

    const WiFiSettings& settings = wifiSettingsManager.get();

    response->printf("模式: %s\n", wifiManager.getModeString().c_str());
    response->printf("狀態: %s\n", wifiManager.isConnected() ? "已連接" : "未連接");
    response->printf("IP 位址: %s\n", wifiManager.getIPAddress().c_str());

    if (settings.mode == WiFiMode::AP || settings.mode == WiFiMode::AP_STA) {
        response->println("");
        response->println("Access Point:");
        response->printf("  SSID: %s\n", settings.ap_ssid);
        response->printf("  Channel: %d\n", settings.ap_channel);
        response->printf("  Clients: %d\n", wifiManager.getClientCount());
    }

    if (settings.mode == WiFiMode::STA || settings.mode == WiFiMode::AP_STA) {
        response->println("");
        response->println("Station:");
        response->printf("  SSID: %s\n", settings.sta_ssid);
        response->printf("  DHCP: %s\n", settings.sta_dhcp ? "Enabled" : "Disabled");
        if (wifiManager.isConnected()) {
            response->printf("  RSSI: %d dBm\n", wifiManager.getRSSI());
        }
    }

    response->println("");
}

void CommandParser::handleWiFiStart(ICommandResponse* response) {
    response->println("🔧 啟動 WiFi...");

    if (wifiManager.start()) {
        response->println("✅ WiFi 啟動成功");
        response->printf("  IP 位址: %s\n", wifiManager.getIPAddress().c_str());
        response->printf("  模式: %s\n", wifiManager.getModeString().c_str());
    } else {
        response->println("❌ WiFi 啟動失敗");
    }
}

void CommandParser::handleWiFiStop(ICommandResponse* response) {
    wifiManager.stop();
    response->println("✅ WiFi 已停止");
}

void CommandParser::handleWiFiScan(ICommandResponse* response) {
    response->println("🔍 掃描 WiFi 網路...");

    int n = wifiManager.scanNetworks();

    if (n <= 0) {
        response->println("⚠️ 未找到網路");
        return;
    }

    response->printf("找到 %d 個網路:\n\n", n);
    response->println("SSID                             | RSSI  | Secure");
    response->println("----------------------------------+-------+--------");

    for (int i = 0; i < n && i < 20; i++) {
        String ssid;
        int8_t rssi;
        bool secure;

        if (wifiManager.getScanResult(i, ssid, rssi, secure)) {
            char line[64];
            snprintf(line, sizeof(line), "%-32s | %4d  | %s",
                    ssid.c_str(), rssi, secure ? "Yes" : "No");
            response->println(line);
        }
    }

    response->println("");
}

void CommandParser::handleWebStatus(ICommandResponse* response) {
    response->println("=== Web 伺服器狀態 ===");

    response->printf("執行中: %s\n", webServerManager.isRunning() ? "是" : "否");
    response->printf("連接埠: %d\n", wifiSettingsManager.get().web_port);
    response->printf("WebSocket 客戶端: %d\n", webServerManager.getWSClientCount());

    if (wifiManager.isConnected()) {
        response->println("");
        response->printf("存取網址: http://%s/\n", wifiManager.getIPAddress().c_str());
    }

    response->println("");
}

void CommandParser::handleWiFiConnect(const String& cmd, ICommandResponse* response) {
    // Parse command: WIFI <ssid> <password>
    // Format: "WIFI ssid password" or "wifi ssid password"

    int firstSpace = cmd.indexOf(' ');
    if (firstSpace == -1) {
        response->println("❌ 格式錯誤");
        response->println("用法: WIFI <ssid> <password>");
        return;
    }

    String remainder = cmd.substring(firstSpace + 1);
    remainder.trim();

    int secondSpace = remainder.indexOf(' ');
    if (secondSpace == -1) {
        response->println("❌ 格式錯誤: 缺少密碼");
        response->println("用法: WIFI <ssid> <password>");
        return;
    }

    String ssid = remainder.substring(0, secondSpace);
    String password = remainder.substring(secondSpace + 1);

    ssid.trim();
    password.trim();

    if (ssid.length() == 0) {
        response->println("❌ SSID 不能為空");
        return;
    }

    // Update WiFi settings
    WiFiSettings& settings = wifiSettingsManager.get();
    strncpy(settings.sta_ssid, ssid.c_str(), sizeof(settings.sta_ssid) - 1);
    settings.sta_ssid[sizeof(settings.sta_ssid) - 1] = '\0';
    strncpy(settings.sta_password, password.c_str(), sizeof(settings.sta_password) - 1);
    settings.sta_password[sizeof(settings.sta_password) - 1] = '\0';
    settings.mode = WiFiMode::STA;  // Set to Station mode

    // Save settings
    wifiSettingsManager.save();

    response->printf("🔧 正在連接到 WiFi: %s\n", ssid.c_str());

    // Stop current WiFi
    wifiManager.stop();
    delay(500);

    // Start WiFi with new settings
    if (wifiManager.start()) {
        // Wait for connection
        int attempts = 0;
        while (!wifiManager.isConnected() && attempts < 30) {
            delay(500);
            attempts++;
        }

        if (wifiManager.isConnected()) {
            response->println("✅ WiFi 連接成功！");
            response->printf("  IP 位址: %s\n", wifiManager.getIPAddress().c_str());
            response->printf("  RSSI: %d dBm\n", wifiManager.getRSSI());

            // Start web server if not running
            if (!webServerManager.isRunning()) {
                if (webServerManager.start()) {
                    response->println("");
                    response->println("🌐 Web 伺服器已啟動");
                    response->printf("  存取網址: http://%s/\n", wifiManager.getIPAddress().c_str());
                }
            }
        } else {
            response->println("❌ WiFi 連接失敗");
            response->println("  請檢查 SSID 和密碼是否正確");
        }
    } else {
        response->println("❌ WiFi 啟動失敗");
    }
}

void CommandParser::handleIPAddress(ICommandResponse* response) {
    response->println("=== IP 位址資訊 ===");

    if (!wifiManager.isConnected()) {
        response->println("⚠️ WiFi 未連接");
        response->println("");
        return;
    }

    const WiFiSettings& settings = wifiSettingsManager.get();

    // Station mode IP
    if (settings.mode == WiFiMode::STA || settings.mode == WiFiMode::AP_STA) {
        response->println("Station Mode:");
        response->printf("  IP 位址: %s\n", wifiManager.getIPAddress().c_str());
        response->printf("  SSID: %s\n", settings.sta_ssid);
        response->printf("  RSSI: %d dBm\n", wifiManager.getRSSI());
    }

    // Access Point IP
    if (settings.mode == WiFiMode::AP || settings.mode == WiFiMode::AP_STA) {
        if (settings.mode == WiFiMode::AP_STA) {
            response->println("");
        }
        response->println("Access Point Mode:");
        response->printf("  IP 位址: %s\n", WiFi.softAPIP().toString().c_str());
        response->printf("  SSID: %s\n", settings.ap_ssid);
        response->printf("  已連接客戶端: %d\n", wifiManager.getClientCount());
    }

    // Web server URL
    if (webServerManager.isRunning()) {
        response->println("");
        response->println("🌐 Web 伺服器:");
        response->printf("  存取網址: http://%s/\n", wifiManager.getIPAddress().c_str());
    }

    response->println("");
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
