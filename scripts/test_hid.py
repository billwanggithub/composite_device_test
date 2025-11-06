#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32-S3 USB HID 測試工具 - 64 位元組，無 Report ID
使用 pywinusb 庫
支援命令解析器測試（*IDN?, HELP, INFO 等）和 0xA1 協定
"""

import pywinusb.hid as hid
import time
import sys
import threading

# ESP32-S3 VID/PID (可通過環境變數或參數覆蓋)
VID = 0x303A
PID = 0x1001  # ESP32-S3 TinyUSB HID 介面的 PID

# 全域變數：接收到的回應
received_responses = []
response_lock = threading.Lock()

def list_devices():
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

def find_device():
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

def data_handler(data):
    """HID IN 報告處理函數"""
    global received_responses

    # data 是接收到的原始數據（含 Report ID）
    # 第一個 byte 是 Report ID
    payload = data[1:]  # 跳過 Report ID

    with response_lock:
        received_responses.append(bytes(payload))

def send_data(device, data):
    """傳送 HID OUT 報告 - 無 Report ID，完整 64 位元組"""
    try:
        # 準備資料（完整 64 位元組，無 Report ID）
        if len(data) < 64:
            data = list(data) + [0x00] * (64 - len(data))
        elif len(data) > 64:
            data = list(data[:64])

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

    except Exception as e:
        print(f"✗ 錯誤: {e}")
        return False

def encode_command_0xA1(cmd_string):
    """
    編碼命令為 0xA1 協定格式
    格式: [Report ID=0][0xA1][length][0x00][command_string...] + padding to 64 bytes
    """
    cmd_bytes = cmd_string.encode('utf-8')
    length = len(cmd_bytes)

    if length > 61:
        print(f"⚠️  命令過長 ({length} bytes)，最多 61 bytes")
        length = 61
        cmd_bytes = cmd_bytes[:61]

    # 建立 65-byte 封包 (1 byte Report ID + 64 bytes data)
    packet = [0]  # Report ID = 0 (無 Report ID)
    packet.append(0xA1)     # 命令類型
    packet.append(length)   # 命令長度
    packet.append(0x00)     # 保留位元
    packet.extend(cmd_bytes)  # 命令內容
    packet.extend([0] * (64 - 3 - length))  # 補零到 64 bytes

    return packet

def decode_response_0xA1(data):
    """
    解碼 0xA1 協定回應封包
    格式: [Report ID=0][0xA1][length][0x00][response_text...]
    """
    if len(data) < 5:
        return None

    # data[0] 是 Report ID (應該是 0)
    # data[1] 應該是 0xA1
    if data[1] != 0xA1:
        return None

    length = data[2]

    # 檢查長度合理性
    if length == 0 or length > 61:
        return None

    # 提取回應文字 (從 data[4] 開始)
    try:
        response = bytes(data[4:4+length]).decode('utf-8', errors='ignore')
        return response
    except:
        return None

def send_command(device, command, use_0xA1_protocol=False):
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

        # 填充到 64 bytes
        data = list(cmd_bytes)
        if len(data) < 64:
            data += [0x00] * (64 - len(data))
        elif len(data) > 64:
            print(f"⚠ 警告: 命令長度 {len(cmd_bytes)} bytes 超過 64，將被截斷")
            data = data[:64]

        print(f"發送命令: {command.strip()}")
        output_report.set_raw_data([0] + data)

    output_report.send()
    return True

def wait_for_response(timeout=2.0, use_0xA1_protocol=False):
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
                        # data 已經是 bytes，需要轉換為 list
                        data_list = [0] + list(data)  # 添加 Report ID
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
                    except:
                        return full_response.hex()

        time.sleep(0.1)

    return None

def test_commands(use_0xA1=False):
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

    # 測試命令列表
    test_cmds = [
        ("*IDN?", "SCPI 識別命令"),
        ("HELP", "顯示說明"),
        ("INFO", "設備資訊"),
        ("STATUS", "系統狀態"),
    ]

    for cmd, description in test_cmds:
        print(f"\n>>> 測試: {cmd} ({description})")
        print("-" * 60)

        if send_command(device, cmd, use_0xA1_protocol=use_0xA1):
            # 等待回應
            time.sleep(0.5)
            response = wait_for_response(timeout=2.0, use_0xA1_protocol=use_0xA1)

            if response:
                print(f"<<< 回應:")
                print(response)
            else:
                print("<<< 未收到回應")

        time.sleep(0.5)

    print("\n" + "=" * 60)
    print("測試完成！")

    # 關閉設備
    device.close()

def receive_data():
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
            except:
                pass
            print()

    device.set_raw_data_handler(print_data)

    try:
        while True:
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\n停止接收")
    finally:
        device.close()

def interactive_mode(use_0xA1=False):
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
                            time.sleep(0.5)
                            response = wait_for_response(timeout=2.0, use_0xA1_protocol=current_protocol)
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
                        time.sleep(0.5)
                        response = wait_for_response(timeout=2.0, use_0xA1_protocol=current_protocol)
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

def main():
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
        print(f"  {sys.argv[0]} cmd *IDN?            - 發送識別命令")
        print(f"  {sys.argv[0]} test                 - 自動測試所有命令")
        print(f"  {sys.argv[0]} interactive          - 互動模式")
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

        # 設置接收處理函數
        device.set_raw_data_handler(data_handler)

        # 發送命令
        if send_command(device, cmd_str):
            time.sleep(0.5)
            response = wait_for_response(timeout=2.0)

            if response:
                print(f"\n回應:")
                print(response)
            else:
                print("\n未收到回應")

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
            send_data(device, data)
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
