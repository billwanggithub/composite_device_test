#include "HIDProtocol.h"

bool HIDProtocol::isCommandPacket(const uint8_t* data, char* command_out, uint8_t* command_len_out) {
    // 檢查 header: [0xA1][length][0x00]
    if (data[0] != TYPE_COMMAND) {
        return false;
    }

    uint8_t cmd_len = data[1];

    // 檢查長度合理性（最多 61 bytes，至少 1 byte）
    if (cmd_len == 0 || cmd_len > 61) {
        return false;
    }

    // 檢查第三個 byte 是否為 0x00（協定保留位元）
    if (data[2] != 0x00) {
        return false;
    }

    // 複製命令字串
    memcpy(command_out, data + 3, cmd_len);
    command_out[cmd_len] = '\0';  // 加上字串結尾符號

    *command_len_out = cmd_len;
    return true;
}

bool HIDProtocol::isPlainTextCommand(const uint8_t* data, char* command_out, uint8_t* command_len_out) {
    // 檢查第一個字元是否為可列印 ASCII（空格到 ~，0x20-0x7E）
    // 排除 0xA0-0xA2（協定標頭）
    if (data[0] < 0x20 || data[0] > 0x7E) {
        return false;
    }

    // 尋找換行符（\n 或 \r）
    uint8_t cmd_len = 0;
    for (uint8_t i = 0; i < 64; i++) {
        if (data[i] == '\n' || data[i] == '\r') {
            cmd_len = i;
            break;
        }
        if (data[i] == 0x00) {
            // 遇到 null terminator，也可能是命令結束
            cmd_len = i;
            break;
        }
    }

    // 檢查命令長度合理性（至少 1 個字元）
    if (cmd_len == 0 || cmd_len > 64) {
        return false;
    }

    // 複製命令字串
    memcpy(command_out, data, cmd_len);
    command_out[cmd_len] = '\0';  // 加上字串結尾符號

    *command_len_out = cmd_len;
    return true;
}

bool HIDProtocol::parseCommand(const uint8_t* data, char* command_out, uint8_t* command_len_out, bool* is_0xA1_protocol) {
    // 先嘗試 0xA1 協定
    if (isCommandPacket(data, command_out, command_len_out)) {
        if (is_0xA1_protocol) {
            *is_0xA1_protocol = true;
        }
        return true;
    }

    // 再嘗試純文本協定
    if (isPlainTextCommand(data, command_out, command_len_out)) {
        if (is_0xA1_protocol) {
            *is_0xA1_protocol = false;
        }
        return true;
    }

    return false;
}

uint8_t HIDProtocol::encodeResponse(uint8_t* out, const uint8_t* payload, uint8_t payload_len) {
    // 限制 payload 長度（最多 61 bytes）
    if (payload_len > 61) {
        payload_len = 61;
    }

    // 編碼 3-byte header
    out[0] = TYPE_COMMAND;  // 0xA1（使用相同的命令類型標記）
    out[1] = payload_len;   // 實際資料長度
    out[2] = 0x00;          // 保留位元

    // 複製 payload
    memcpy(out + 3, payload, payload_len);

    // 補零到 64 bytes
    if (payload_len < 61) {
        memset(out + 3 + payload_len, 0, 64 - 3 - payload_len);
    }

    return 64;  // 固定回傳 64 bytes
}
