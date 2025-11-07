#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <Arduino.h>

// 命令來源類型
enum CommandSource {
    CMD_SOURCE_CDC,        // CDC 序列埠
    CMD_SOURCE_HID,        // HID 介面
    CMD_SOURCE_BLE         // BLE GATT 介面
};

// 命令回應介面
class ICommandResponse {
public:
    virtual void print(const char* str) = 0;
    virtual void println(const char* str) = 0;
    virtual void printf(const char* format, ...) = 0;
};

// 命令解析器類別
class CommandParser {
public:
    CommandParser();

    // 處理單一命令（必須以 \n 結尾）
    // 返回 true 表示命令已處理
    bool processCommand(const String& cmd, ICommandResponse* response, CommandSource source);

    // 添加字元到緩衝區，自動處理換行和命令執行
    // 返回 true 表示有完整命令被處理
    bool feedChar(char c, String& buffer, ICommandResponse* response, CommandSource source);

    // 檢查命令是否為 SCPI 命令
    static bool isSCPICommand(const String& cmd);

private:
    void handleIDN(ICommandResponse* response);
    void handleHelp(ICommandResponse* response);
    void handleInfo(ICommandResponse* response);
    void handleStatus(ICommandResponse* response);
    void handleSend(ICommandResponse* response);
    void handleRead(ICommandResponse* response);
    void handleClear(ICommandResponse* response);
    void handleDelay(const String& cmd, ICommandResponse* response);

    // Motor control command handlers
    void handleSetPWMFreq(ICommandResponse* response, uint32_t freq);
    void handleSetPWMDuty(ICommandResponse* response, float duty);
    void handleSetPolePairs(ICommandResponse* response, uint8_t pairs);
    void handleSetMaxFreq(ICommandResponse* response, uint32_t maxFreq);
    void handleSetMaxRPM(ICommandResponse* response, uint32_t maxRPM);
    void handleSetLEDBrightness(ICommandResponse* response, uint8_t brightness);
    void handleRPM(ICommandResponse* response);
    void handleMotorStatus(ICommandResponse* response);
    void handleMotorStop(ICommandResponse* response);
    void handleSaveSettings(ICommandResponse* response);
    void handleLoadSettings(ICommandResponse* response);
    void handleResetSettings(ICommandResponse* response);

    // Advanced features (Priority 3)
    void handleSetPWMFreqRamped(ICommandResponse* response, uint32_t freq, uint32_t rampTimeMs);
    void handleSetPWMDutyRamped(ICommandResponse* response, float duty, uint32_t rampTimeMs);
    void handleSetRPMFilterSize(ICommandResponse* response, uint8_t size);
    void handleFilterStatus(ICommandResponse* response);

    // WiFi and Web Server commands (WiFi Web Server feature)
    void handleWiFiConnect(const String& cmd, ICommandResponse* response);
    void handleIPAddress(ICommandResponse* response);
    void handleWiFiStatus(ICommandResponse* response);
    void handleWiFiStart(ICommandResponse* response);
    void handleWiFiStop(ICommandResponse* response);
    void handleWiFiScan(ICommandResponse* response);
    void handleWebStatus(ICommandResponse* response);

    // Peripheral commands (UART, Buzzer, LED, Relay, GPIO, Keys)
    void handleUART1Mode(const String& cmd, ICommandResponse* response);
    void handleUART1Config(const String& cmd, ICommandResponse* response);
    void handleUART1PWM(const String& cmd, ICommandResponse* response);
    void handleUART1Status(ICommandResponse* response);
    void handleUART1Write(const String& cmd, ICommandResponse* response);
    void handleUART2Config(const String& cmd, ICommandResponse* response);
    void handleUART2Status(ICommandResponse* response);
    void handleUART2Write(const String& cmd, ICommandResponse* response);
    void handleBuzzerControl(const String& cmd, ICommandResponse* response);
    void handleBuzzerBeep(const String& cmd, ICommandResponse* response);
    void handleLEDPWM(const String& cmd, ICommandResponse* response);
    void handleLEDFade(const String& cmd, ICommandResponse* response);
    void handleRelayControl(const String& cmd, ICommandResponse* response);
    void handleGPIOControl(const String& cmd, ICommandResponse* response);
    void handleKeysStatus(ICommandResponse* response);
    void handleKeysConfig(const String& cmd, ICommandResponse* response);
    void handleKeysMode(const String& cmd, ICommandResponse* response);
    void handlePeripheralStatus(ICommandResponse* response);
    void handlePeripheralStats(ICommandResponse* response);

    // Peripheral settings commands
    void handlePeripheralSave(ICommandResponse* response);
    void handlePeripheralLoad(ICommandResponse* response);
    void handlePeripheralReset(ICommandResponse* response);
};

// CDC 回應實作
class CDCResponse : public ICommandResponse {
public:
    CDCResponse(USBCDC& serial) : _serial(serial) {}

    void print(const char* str) override {
        _serial.print(str);
    }

    void println(const char* str) override {
        _serial.println(str);
    }

    void printf(const char* format, ...) override {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        _serial.print(buffer);
    }

private:
    USBCDC& _serial;
};

// HID 回應實作
class HIDResponse : public ICommandResponse {
public:
    HIDResponse(void* hid_instance) : _hid(hid_instance) {}

    void print(const char* str) override {
        sendString(str);
    }

    void println(const char* str) override {
        sendString(str);
        sendString("\n");
    }

    void printf(const char* format, ...) override {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        sendString(buffer);
    }

private:
    void* _hid;

    void sendString(const char* str);
};

// 多通道回應實作（同時輸出到多個介面）
class MultiChannelResponse : public ICommandResponse {
public:
    MultiChannelResponse(ICommandResponse* channel1, ICommandResponse* channel2)
        : _channel1(channel1), _channel2(channel2) {}

    void print(const char* str) override {
        if (_channel1) _channel1->print(str);
        if (_channel2) _channel2->print(str);
    }

    void println(const char* str) override {
        if (_channel1) _channel1->println(str);
        if (_channel2) _channel2->println(str);
    }

    void printf(const char* format, ...) override {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        print(buffer);
    }

private:
    ICommandResponse* _channel1;
    ICommandResponse* _channel2;
};

// BLE 回應實作（透過 BLE GATT TX Characteristic）
class BLEResponse : public ICommandResponse {
public:
    BLEResponse(void* ble_characteristic) : _characteristic(ble_characteristic) {}

    void print(const char* str) override;
    void println(const char* str) override;
    void printf(const char* format, ...) override;

private:
    void* _characteristic;  // BLECharacteristic* (避免在 header 中引入 BLE 相依性)
};

#endif // COMMAND_PARSER_H
