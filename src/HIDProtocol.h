#ifndef HID_PROTOCOL_H
#define HID_PROTOCOL_H

#include <Arduino.h>

/**
 * HID 協定處理類別
 *
 * 封包格式：
 * - 命令封包：[0xA1][length][0x00][command_string...]
 * - Header 長度：3 bytes
 * - Command string 最大長度：61 bytes
 * - 總長度：固定 64 bytes
 */
class HIDProtocol {
public:
    // 封包類型定義
    static const uint8_t TYPE_COMMAND = 0xA1;    // 命令封包
    static const uint8_t TYPE_DATA = 0xA0;       // 原始資料（保留供未來使用）
    static const uint8_t TYPE_RESPONSE = 0xA2;   // 命令回應（保留供未來使用）

    /**
     * 檢查是否為命令封包（0xA1 header）
     *
     * @param data 64-byte HID 封包
     * @param command_out 輸出緩衝區，用於存放命令字串（至少 62 bytes）
     * @param command_len_out 輸出命令長度
     * @return true 表示是命令封包，command_out 會填入命令字串
     */
    static bool isCommandPacket(const uint8_t* data, char* command_out, uint8_t* command_len_out);

    /**
     * 檢查是否為純文本命令（無標頭，直接以可列印字元開始）
     *
     * @param data 64-byte HID 封包
     * @param command_out 輸出緩衝區，用於存放命令字串（至少 65 bytes）
     * @param command_len_out 輸出命令長度
     * @return true 表示是純文本命令，command_out 會填入命令字串
     */
    static bool isPlainTextCommand(const uint8_t* data, char* command_out, uint8_t* command_len_out);

    /**
     * 自動偵測並解析命令（支援 0xA1 協定和純文本協定）
     *
     * @param data 64-byte HID 封包
     * @param command_out 輸出緩衝區，用於存放命令字串（至少 65 bytes）
     * @param command_len_out 輸出命令長度
     * @param is_0xA1_protocol 輸出參數，表示是否為 0xA1 協定
     * @return true 表示成功解析命令，command_out 會填入命令字串
     */
    static bool parseCommand(const uint8_t* data, char* command_out, uint8_t* command_len_out, bool* is_0xA1_protocol);

    /**
     * 編碼回應封包（加上 3-byte header）
     *
     * @param out 輸出緩衝區，至少 64 bytes
     * @param payload 實際資料
     * @param payload_len 資料長度（最多 61 bytes）
     * @return 實際編碼後的長度（固定 64）
     */
    static uint8_t encodeResponse(uint8_t* out, const uint8_t* payload, uint8_t payload_len);
};

#endif // HID_PROTOCOL_H
