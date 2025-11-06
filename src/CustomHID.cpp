#include "CustomHID.h"

#if CONFIG_TINYUSB_HID_ENABLED

#include "class/hid/hid.h"
#include "class/hid/hid_device.h"

// HID 報告描述符 - 64 位元組 IN/OUT，不使用 Report ID
// 這樣可以在單個 64-byte USB 包中傳輸完整的 64 bytes 數據
#define CUSTOM_HID_REPORT_DESC_64() \
    HID_USAGE_PAGE_N ( HID_USAGE_PAGE_VENDOR, 2   ), \
    HID_USAGE        ( 0x01                       ), \
    HID_COLLECTION   ( HID_COLLECTION_APPLICATION ), \
      /* Input Report - 64 bytes */ \
      HID_USAGE       ( 0x02                                   ), \
      HID_LOGICAL_MIN ( 0x00                                   ), \
      HID_LOGICAL_MAX ( 0xff                                   ), \
      HID_REPORT_SIZE ( 8                                      ), \
      HID_REPORT_COUNT( 64                                     ), \
      HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ), \
      /* Output Report - 64 bytes */ \
      HID_USAGE       ( 0x03                                    ), \
      HID_LOGICAL_MIN ( 0x00                                    ), \
      HID_LOGICAL_MAX ( 0xff                                    ), \
      HID_REPORT_SIZE ( 8                                       ), \
      HID_REPORT_COUNT( 64                                      ), \
      HID_OUTPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE  ), \
    HID_COLLECTION_END

#define CUSTOM_HID_REPORT_DESC_64_LEN 32  // 完整描述符長度（含 OUTPUT 參數和 END_COLLECTION）

CustomHID64::CustomHID64(void) : hid() {
    data_available = false;
    data_callback = nullptr;
    last_report_id = 0;
    last_raw_len = 0;
    memset(rx_buffer, 0, sizeof(rx_buffer));
}

void CustomHID64::begin(void) {
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        hid.addDevice(this, CUSTOM_HID_REPORT_DESC_64_LEN);
    }
    hid.begin();
}

void CustomHID64::end(void) {
    hid.end();
}

uint16_t CustomHID64::_onGetDescriptor(uint8_t* dst) {
    // 創建報告描述符（無 Report ID）
    uint8_t report_descriptor[] = {
        CUSTOM_HID_REPORT_DESC_64()
    };
    memcpy(dst, report_descriptor, sizeof(report_descriptor));
    return sizeof(report_descriptor);
}

void CustomHID64::_onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) {
    // 接收 HID OUT 報告（從電腦傳來的資料）
    // 沒有 Report ID，直接接收 64 bytes 數據

    // 保存調試資訊
    last_report_id = report_id;
    last_raw_len = len;

    // 直接複製所有資料（最多 64 bytes）
    size_t copy_len = (len > 64) ? 64 : len;
    memcpy(rx_buffer, buffer, copy_len);

    // 如果不足 64 位元組，補零
    if (len < 64) {
        memset(rx_buffer + len, 0, 64 - len);
    }

    data_available = true;

    if (data_callback != nullptr) {
        // 傳遞實際接收的長度（而不是固定的 64）
        data_callback(rx_buffer, len);
    }
}

bool CustomHID64::send(const uint8_t* data, size_t len) {
    // 傳送 HID IN 報告（傳給電腦的資料）
    // 沒有 Report ID，直接發送 64 bytes

    if (len > 64) {
        len = 64;  // 最多 64 位元組
    }

    // 準備 64 位元組緩衝區
    uint8_t buffer[64] = {0};

    // 複製使用者資料
    memcpy(buffer, data, len);

    // 如果不足 64 位元組，補零
    if (len < 64) {
        memset(buffer + len, 0, 64 - len);
    }

    // 發送 64 位元組（無 Report ID，使用 0）
    return hid.SendReport(0, buffer, 64);
}

void CustomHID64::onData(DataCallback callback) {
    data_callback = callback;
}

#endif /* CONFIG_TINYUSB_HID_ENABLED */
