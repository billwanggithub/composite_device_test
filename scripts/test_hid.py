#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32-S3 USB HID 測試工具 - 64 位元組，無 Report ID
使用 pywinusb 庫
支援命令解析器測試（*IDN?, HELP, INFO 等）和 0xA1 協定

重要：回應路由規則（v2.2）
----------------------------
根據命令類型，裝置會將回應路由到不同介面：

1. SCPI 命令（*IDN?, *RST 等）：
   - 回應到命令來源介面（HID → HID 回應）
   - 測試這些命令時會收到 HID 回應

2. 一般命令（HELP, INFO, STATUS 等）：
   - 統一回應到 CDC 介面（便於監控除錯）
   - 測試這些命令時不會收到 HID 回應（需查看 CDC 輸出）

這是設計行為，不是 bug！
"""

import pywinusb.hid as hid
import time
import sys
import threading
import os
from typing import List, Optional, Tuple

# ==================== 配置常數 ====================
# ESP32-S3 VID/PID (可通過環境變數覆蓋)
VID = int(os.environ.get('ESP32_VID', '0x303A'), 16)
PID = int(os.environ.get('ESP32_PID', '0x1001'), 16)

# HID 封包參數
HID_PACKET_SIZE = 64  # HID 資料封包大小（不含 Report ID）
PROTOCOL_0xA1_HEADER_SIZE = 3  # 0xA1 協定標頭大小（0xA1 + length + 0x00）
MAX_COMMAND_LENGTH = HID_PACKET_SIZE - PROTOCOL_0xA1_HEADER_SIZE  # 61 bytes

# 超時設定
DEFAULT_RESPONSE_TIMEOUT = 2.0  # 等待回應的預設超時（秒）
SEND_COMMAND_DELAY = 0.5  # 發送命令後的等待時間（秒）
POLL_INTERVAL = 0.1  # 輪詢間隔（秒）

# ==================== 全域變數 ====================
# 接收到的回應
received_responses = []
response_lock = threading.Lock()

def list_devices() -> bool:
    """列出所有 HID 裝置"""
    print("=" * 60)
    print("可用的 HID 裝置:")
    print("=" * 60)

    found = False
    all_devices = hid.find_all_hid_devices()

    for device in all_devices:
        if device.vendor_id == VID and device.product_id == PID:
            print(f">>> 找到目標裝置 <<<")
            found = True

        print(f"VID: 0x{device.vendor_id:04X}, PID: 0x{device.product_id:04X}")
        print(f"  路徑: {device.device_path}")
        print(f"  產品: {device.product_name}")
        print(f"  製造商: {device.vendor_name}")
        print()

    return found

def find_device() -> Optional[hid.HidDevice]:
    """尋找 ESP32-S3 HID 設備"""
    all_devices = hid.HidDeviceFilter(vendor_id=VID, product_id=PID).get_devices()

    if not all_devices:
        print(f"找不到設備 {VID:04X}:{PID:04X}")
        return None

    # 找到 HID 介面（不是 CDC）
    for device in all_devices:
        # 檢查是否為 HID 介面
        if "hid" in device.device_path.lower():
            return device

    return all_devices[0] if all_devices else None

def data_handler(data: List[int]) -> None:
    """
    HID IN 報告處理函數

    注意：雖然 HID 描述符沒有 Report ID，但 pywinusb 仍會在 data[0] 加入
    Report ID (值為 0)，實際 HID 資料從 data[1] 開始（64 bytes）
    """
    global received_responses

    # data[0] = Report ID (pywinusb 自動添加，值為 0)
    # data[1:65] = 實際 HID 資料（64 bytes）
    payload = data[1:]  # 提取 64-byte HID 資料

    with response_lock:
        received_responses.append(bytes(payload))

def send_data(device: hid.HidDevice, data: List[int]) -> bool:
    """傳送 HID OUT 報告 - 無 Report ID，完整 64 位元組"""
    try:
        # 準備資料（完整 HID_PACKET_SIZE 位元組，無 Report ID）
        if len(data) < HID_PACKET_SIZE:
            data = list(data) + [0x00] * (HID_PACKET_SIZE - len(data))
        elif len(data) > HID_PACKET_SIZE:
            data = list(data[:HID_PACKET_SIZE])

        print(f"傳送 {len(data)} 位元組（無 Report ID）:")
        for i in range(0, min(len(data), 32), 16):
            print(f"  {i:04X}: " + ' '.join(f'{b:02X}' for b in data[i:i+16]))
        if len(data) > 32:
            print(f"  ... (共 {len(data)} bytes)")

        # 發送
        report = device.find_output_reports()
        if not report:
            print("✗ 找不到 output report")
            return False

        output_report = report[0]
        # pywinusb 需要加上 Report ID (0)
        output_report.set_raw_data([0] + data)
        output_report.send()

        print(f"✓ 成功傳送 {len(data)} 位元組！")
        return True

    except AttributeError as e:
        print(f"✗ 設備錯誤: {e}")
        return False
    except OSError as e:
        print(f"✗ USB 通訊錯誤: {e}")
        return False
    except Exception as e:
        print(f"✗ 未知錯誤: {e}")
        return False

def encode_command_0xA1(cmd_string: str) -> List[int]:
    """
    編碼命令為 0xA1 協定格式

    pywinusb 格式: [Report ID][64-byte HID data]
    0xA1 協定格式: [0xA1][length][0x00][command_string...][padding]

    返回: 65-byte list = [0] + [0xA1, length, 0x00, cmd_bytes, padding...]
                         ↑        ↑________________________↑
                      Report ID            64 bytes
    """
    cmd_bytes = cmd_string.encode('utf-8')
    length = len(cmd_bytes)

    if length > MAX_COMMAND_LENGTH:
        print(f"⚠️  命令過長 ({length} bytes)，最多 {MAX_COMMAND_LENGTH} bytes")
        length = MAX_COMMAND_LENGTH
        cmd_bytes = cmd_bytes[:MAX_COMMAND_LENGTH]

    # 建立 65-byte 封包給 pywinusb (1 + HID_PACKET_SIZE)
    packet = [0]  # [0] Report ID (pywinusb 需要，但實際不傳送到 USB)
    packet.append(0xA1)     # [1] 0xA1 協定標記
    packet.append(length)   # [2] 命令長度
    packet.append(0x00)     # [3] 保留位元
    packet.extend(cmd_bytes)  # [4:4+length] 命令內容
    packet.extend([0] * (HID_PACKET_SIZE - PROTOCOL_0xA1_HEADER_SIZE - length))  # 補零

    return packet

def decode_response_0xA1(data: List[int]) -> Optional[str]:
    """
    解碼 0xA1 協定回應封包

    輸入 data 格式（由呼叫者添加 Report ID）:
    [0] Report ID = 0
    [1] 0xA1 協定標記
    [2] length (回應長度，1-61)
    [3] 0x00 保留位元
    [4:4+length] 回應文字
    """
    if len(data) < 5:
        return None

    # data[0] 是 Report ID (pywinusb 格式，應該是 0)
    # data[1] 應該是 0xA1 協定標記
    if data[1] != 0xA1:
        return None

    length = data[2]

    # 檢查長度合理性（最大 MAX_COMMAND_LENGTH bytes）
    if length == 0 or length > MAX_COMMAND_LENGTH:
        return None

    # 提取回應文字 (從 data[4] 開始，長度為 length)
    try:
        response = bytes(data[4:4+length]).decode('utf-8', errors='ignore')
        return response
    except (UnicodeDecodeError, IndexError, TypeError):
        # 解碼失敗、索引錯誤或類型錯誤
        return None

def send_command(device: hid.HidDevice, command: str, use_0xA1_protocol: bool = False) -> bool:
    """發送命令到 HID 設備（自動加上 \\n）"""
    global received_responses

    # 清空接收緩衝
    with response_lock:
        received_responses.clear()

    # 發送
    report = device.find_output_reports()
    if not report:
        print("✗ 找不到 output report")
        return False

    output_report = report[0]

    if use_0xA1_protocol:
        # 使用 0xA1 協定
        print(f"發送命令 (0xA1 協定): {command.strip()}")
        packet = encode_command_0xA1(command)
        output_report.set_raw_data(packet)
    else:
        # 使用簡單協定（純文字 + 換行符）
        # 確保命令以換行符結尾
        if not command.endswith('\n'):
            command += '\n'

        # 轉換為 bytes
        cmd_bytes = command.encode('ascii')

        # 填充到 HID_PACKET_SIZE bytes
        data = list(cmd_bytes)
        if len(data) < HID_PACKET_SIZE:
            data += [0x00] * (HID_PACKET_SIZE - len(data))
        elif len(data) > HID_PACKET_SIZE:
            print(f"⚠ 警告: 命令長度 {len(cmd_bytes)} bytes 超過 {HID_PACKET_SIZE}，將被截斷")
            data = data[:HID_PACKET_SIZE]

        print(f"發送命令: {command.strip()}")
        output_report.set_raw_data([0] + data)

    output_report.send()
    return True

def wait_for_response(timeout: float = DEFAULT_RESPONSE_TIMEOUT, use_0xA1_protocol: bool = False) -> Optional[str]:
    """等待並返回回應"""
    global received_responses

    start_time = time.time()
    responses = []

    while (time.time() - start_time) < timeout:
        with response_lock:
            if received_responses:
                if use_0xA1_protocol:
                    # 解碼 0xA1 協定回應
                    for data in received_responses:
                        # data 是 64-byte HID 資料（data_handler 已移除 Report ID）
                        # decode_response_0xA1() 期望含 Report ID 的格式，所以補回 [0]
                        data_list = [0] + list(data)  # 添加 Report ID 以符合解碼函數格式
                        response = decode_response_0xA1(data_list)
                        if response:
                            responses.append(response)
                    received_responses.clear()
                    if responses:
                        return '\n'.join(responses)
                else:
                    # 組合所有回應
                    full_response = b''.join(received_responses)
                    received_responses.clear()
                    # 移除尾部的零
                    full_response = full_response.rstrip(b'\x00')
                    # 轉換為字串
                    try:
                        return full_response.decode('ascii')
                    except UnicodeDecodeError:
                        # 解碼失敗，返回十六進位
                        return full_response.hex()

        time.sleep(POLL_INTERVAL)

    return None

def test_commands(use_0xA1: bool = False) -> None:
    """測試所有命令"""
    print("搜尋 HID 設備...")
    device = find_device()

    if not device:
        print("找不到設備！")
        sys.exit(1)

    # 開啟設備
    device.open()
    print(f"\n已開啟設備 {VID:04X}:{PID:04X}")
    print(f"  製造商: {device.vendor_name}")
    print(f"  產品: {device.product_name}")
    print()

    # 設置接收處理函數
    device.set_raw_data_handler(data_handler)

    protocol_name = "0xA1 協定" if use_0xA1 else "簡單協定"
    print("=" * 60)
    print(f"測試命令解析器 ({protocol_name})")
    print("=" * 60)
    print("\n⚠️  重要提示：")
    print("  - SCPI 命令（*IDN? 等）→ HID 會收到回應")
    print("  - 一般命令（HELP, INFO 等）→ 只有 CDC 會收到回應")
    print("  - 如果測試一般命令時無回應，請查看 CDC 序列埠輸出\n")

    # 測試命令列表（區分 SCPI 和一般命令）
    test_cmds = [
        ("*IDN?", "SCPI 識別命令", True),   # SCPI 命令 - 會有 HID 回應
        ("HELP", "顯示說明", False),         # 一般命令 - 只有 CDC 回應
        ("INFO", "設備資訊", False),         # 一般命令 - 只有 CDC 回應
        ("STATUS", "系統狀態", False),       # 一般命令 - 只有 CDC 回應
    ]

    for cmd, description, expects_hid_response in test_cmds:
        print(f"\n>>> 測試: {cmd} ({description})")
        if not expects_hid_response:
            print("    ⚠️  此命令只會回應到 CDC，HID 不會收到回應")
        print("-" * 60)

        if send_command(device, cmd, use_0xA1_protocol=use_0xA1):
            # 等待回應
            time.sleep(SEND_COMMAND_DELAY)
            response = wait_for_response(timeout=DEFAULT_RESPONSE_TIMEOUT, use_0xA1_protocol=use_0xA1)

            if response:
                print(f"<<< HID 回應:")
                print(response)
            else:
                if expects_hid_response:
                    print("<<< 未收到 HID 回應（異常）")
                else:
                    print("<<< 未收到 HID 回應（預期行為，回應已發送到 CDC）")

        time.sleep(SEND_COMMAND_DELAY)

    print("\n" + "=" * 60)
    print("測試完成！")

    # 關閉設備
    device.close()

def receive_data() -> None:
    """接收 HID IN 報告"""
    print("搜尋 HID 設備...")
    device = find_device()

    if not device:
        print("找不到設備！")
        return

    device.open()
    print(f"已開啟設備，等待接收資料（按 Ctrl+C 停止）...")
    print("在 ESP32 序列埠輸入 'send' 命令來傳送測試資料\n")

    def print_data(data):
        payload = data[1:]  # 跳過 Report ID
        non_zero = [b for b in payload if b != 0]

        if non_zero:
            print(f"\n接收到 {len(payload)} 位元組:")
            for i in range(0, len(payload), 16):
                print(f"  {i:04X}: " + ' '.join(f'{b:02X}' for b in payload[i:i+16]))

            # 嘗試解碼為 ASCII
            try:
                text = bytes(payload).rstrip(b'\x00').decode('ascii')
                if text:
                    print(f"  文字: {text}")
            except UnicodeDecodeError:
                # 無法解碼為 ASCII，跳過文字顯示
                pass
            print()

    device.set_raw_data_handler(print_data)

    try:
        while True:
            time.sleep(POLL_INTERVAL)
    except KeyboardInterrupt:
        print("\n停止接收")
    finally:
        device.close()

def interactive_mode(use_0xA1: bool = False) -> None:
    """互動模式 - 支援命令和十六進位資料"""
    protocol_name = "0xA1 協定" if use_0xA1 else "簡單協定"
    print("\n" + "=" * 60)
    print(f"互動模式 - HID 命令和資料測試 ({protocol_name})")
    print("=" * 60)
    print("模式:")
    print("  <命令>          - 直接發送命令（例: *IDN?）")
    print("  cmd: <命令>     - 發送命令（例: cmd: HELP）")
    print("  hex: <資料>     - 發送十六進位資料（例: hex: 12 34 56）")
    print("  protocol        - 切換協定")
    print("  輸入 'q' 離開")
    print()
    print("⚠️  回應路由提示:")
    print("  - SCPI 命令（*開頭）→ HID 會收到回應")
    print("  - 一般命令      → 只有 CDC 會收到回應")
    print()

    print("搜尋 HID 設備...")
    device = find_device()

    if not device:
        print("找不到設備！")
        return

    device.open()
    print(f"✓ 已連接到設備")
    print(f"  製造商: {device.vendor_name}")
    print(f"  產品: {device.product_name}")
    print()

    # 設置接收處理函數
    device.set_raw_data_handler(data_handler)

    current_protocol = use_0xA1

    try:
        while True:
            try:
                user_input = input(">>> ").strip()

                if user_input.lower() in ['q', 'quit', 'exit']:
                    break

                if not user_input:
                    continue

                if user_input.lower() == 'protocol':
                    # 切換協定
                    current_protocol = not current_protocol
                    protocol_name = "0xA1 協定" if current_protocol else "簡單協定"
                    print(f"切換到: {protocol_name}")
                    continue

                if user_input.startswith('cmd:'):
                    # 命令模式
                    command = user_input[4:].strip()
                    if command:
                        if send_command(device, command, use_0xA1_protocol=current_protocol):
                            time.sleep(SEND_COMMAND_DELAY)
                            response = wait_for_response(timeout=DEFAULT_RESPONSE_TIMEOUT, use_0xA1_protocol=current_protocol)
                            if response:
                                print(f"<<< {response}")
                            else:
                                print("<<< (無回應)")

                elif user_input.startswith('hex:'):
                    # 十六進位模式
                    hex_str = user_input[4:].strip()
                    hex_values = hex_str.replace(',', ' ').split()
                    data = [int(h, 16) for h in hex_values if h]

                    if data:
                        send_data(device, data)
                    else:
                        print("請輸入至少一個位元組")

                else:
                    # 預設為命令模式
                    if send_command(device, user_input, use_0xA1_protocol=current_protocol):
                        time.sleep(SEND_COMMAND_DELAY)
                        response = wait_for_response(timeout=DEFAULT_RESPONSE_TIMEOUT, use_0xA1_protocol=current_protocol)
                        if response:
                            print(f"<<< {response}")
                        else:
                            print("<<< (無回應)")

                print()

            except ValueError as e:
                print(f"✗ 錯誤: {e}")
            except KeyboardInterrupt:
                print("\n中斷")
                break

    finally:
        device.close()

def main() -> None:
    print("=" * 60)
    print("ESP32-S3 USB HID 測試工具 - 64 位元組（無 Report ID）")
    print("使用 pywinusb 庫，支援簡單協定和 0xA1 協定")
    print("=" * 60)
    print()

    if len(sys.argv) < 2:
        print("用法:")
        print(f"  {sys.argv[0]} list                 - 列出所有 HID 裝置")
        print(f"  {sys.argv[0]} cmd <command>        - 發送命令 (例: cmd *IDN?)")
        print(f"  {sys.argv[0]} test                 - 測試所有命令（簡單協定）")
        print(f"  {sys.argv[0]} test-0xa1            - 測試所有命令（0xA1 協定）")
        print(f"  {sys.argv[0]} send <hex bytes>     - 傳送資料 (例: send 12 34 56)")
        print(f"  {sys.argv[0]} interactive          - 互動模式（支援命令和資料，簡單協定）")
        print(f"  {sys.argv[0]} interactive-0xa1     - 互動模式（0xA1 協定）")
        print(f"  {sys.argv[0]} receive              - 接收資料模式")
        print()
        print("範例:")
        print(f"  {sys.argv[0]} cmd *IDN?            - 發送識別命令（SCPI，會有 HID 回應）")
        print(f"  {sys.argv[0]} test                 - 自動測試所有命令")
        print(f"  {sys.argv[0]} interactive          - 互動模式")
        print()
        print("⚠️  注意：一般命令（HELP, INFO 等）只會回應到 CDC，不會回應到 HID")
        print("   SCPI 命令（*IDN? 等）會回應到 HID")
        return

    command = sys.argv[1].lower()

    if command == 'list':
        found = list_devices()
        if not found:
            print(f"⚠ 未找到 VID=0x{VID:04X}, PID=0x{PID:04X} 的裝置")
            print("\n請檢查:")
            print("1. ESP32-S3 已連接")
            print("2. 韌體已正確上傳")
            print("3. USB 驅動已正確安裝")

    elif command == 'cmd':
        if len(sys.argv) < 3:
            print("✗ 請提供命令")
            print(f"範例: {sys.argv[0]} cmd *IDN?")
            print(f"      {sys.argv[0]} cmd HELP")
            return

        # 組合剩餘參數為命令
        cmd_str = ' '.join(sys.argv[2:])

        print("搜尋 HID 設備...")
        device = find_device()

        if not device:
            print("找不到設備！")
            return

        device.open()
        print(f"已開啟設備 {VID:04X}:{PID:04X}")

        try:
            # 設置接收處理函數
            device.set_raw_data_handler(data_handler)

            # 發送命令
            if send_command(device, cmd_str):
                time.sleep(SEND_COMMAND_DELAY)
                response = wait_for_response(timeout=DEFAULT_RESPONSE_TIMEOUT)

                if response:
                    print(f"\n回應:")
                    print(response)
                else:
                    print("\n未收到回應")
        finally:
            device.close()

    elif command == 'test':
        test_commands(use_0xA1=False)

    elif command == 'test-0xa1':
        test_commands(use_0xA1=True)

    elif command == 'send':
        if len(sys.argv) < 3:
            print("✗ 請提供要傳送的資料")
            print(f"範例: {sys.argv[0]} send 12 34 56 78")
            return

        # 解析十六進位資料
        try:
            data = [int(h, 16) for h in sys.argv[2:]]

            print("搜尋 HID 設備...")
            device = find_device()

            if not device:
                print("找不到設備！")
                return

            device.open()
            try:
                send_data(device, data)
            finally:
                device.close()

        except ValueError as e:
            print(f"✗ 無效的十六進位格式: {e}")

    elif command == 'interactive':
        interactive_mode(use_0xA1=False)

    elif command == 'interactive-0xa1' or command == 'i-0xa1':
        interactive_mode(use_0xA1=True)

    elif command == 'receive':
        receive_data()

    else:
        print(f"✗ 未知命令: {command}")

if __name__ == '__main__':
    main()
