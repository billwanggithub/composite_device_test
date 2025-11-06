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

private:
    void handleIDN(ICommandResponse* response);
    void handleHelp(ICommandResponse* response);
    void handleInfo(ICommandResponse* response);
    void handleStatus(ICommandResponse* response);
    void handleSend(ICommandResponse* response);
    void handleRead(ICommandResponse* response);
    void handleClear(ICommandResponse* response);
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

// Bluetooth Serial 回應實作
// Note: Classic Bluetooth SPP (BluetoothSerial) is not supported on ESP32-S3.
// BLE GATT is used for serial/console transport instead. No BTSerialResponse
// is provided here to avoid depending on Classic BT symbols.

#endif // COMMAND_PARSER_H
