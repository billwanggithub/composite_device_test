#pragma once
#include "USBHID.h"

#if CONFIG_TINYUSB_HID_ENABLED

#include "esp_event.h"

// 自訂 HID 裝置類別 - 64 位元組，無 Report ID
class CustomHID64 : public USBHIDDevice {
private:
    USBHID hid;

    // 接收緩衝區
    uint8_t rx_buffer[64];
    volatile bool data_available;

    // 事件回調
    typedef void (*DataCallback)(const uint8_t* data, uint16_t len);
    DataCallback data_callback;

    // 調試資訊
    uint8_t last_report_id;
    uint16_t last_raw_len;

public:
    CustomHID64(void);

    void begin(void);
    void end(void);

    // 傳送 64 位元組資料（HID IN 報告）
    bool send(const uint8_t* data, size_t len);

    // 設定接收資料的回調函數
    void onData(DataCallback callback);

    // 取得調試資訊
    uint8_t getLastReportId() { return last_report_id; }
    uint16_t getLastRawLen() { return last_raw_len; }

    // USBHIDDevice 介面實作
    uint16_t _onGetDescriptor(uint8_t* buffer) override;
    void _onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) override;
};

#endif /* CONFIG_TINYUSB_HID_ENABLED */
