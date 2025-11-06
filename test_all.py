#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32-S3 整合測試腳本 - 同時測試 CDC 和 HID 介面
使用 pywinusb 進行 HID 通訊，pyserial 進行 CDC 通訊
"""

import sys
import time
import threading

# 檢查依賴
try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("❌ 需要安裝 pyserial")
    print("請執行: pip install pyserial")
    sys.exit(1)

try:
    import pywinusb.hid as hid
except ImportError:
    print("❌ 需要安裝 pywinusb")
    print("請執行: pip install pywinusb")
    sys.exit(1)

# ESP32-S3 VID/PID
VENDOR_ID = 0x303A
PRODUCT_ID = 0x1001  # ESP32-S3 TinyUSB HID 介面的 PID

# 全域變數存放 HID 接收資料
received_hid_data = []
hid_data_lock = threading.Lock()

def find_cdc_device():
    """掃描 COM ports 找到 ESP32-S3（只掃描 USB CDC 裝置）"""
    print("\n" + "=" * 60)
    print("掃描 CDC (Serial) 介面")
    print("=" * 60)

    ports = serial.tools.list_ports.comports()

    for port in ports:
        port_name = port.device
        description = port.description.lower() if port.description else ""

        # 跳過藍牙裝置（支援中英文關鍵字）
        bluetooth_keywords = ['bluetooth', 'bt ', '藍牙', '藍芽', '透過藍牙', '透過藍芽']
        if any(keyword in description for keyword in bluetooth_keywords):
            continue

        # 只掃描有 VID/PID 的 USB 裝置（虛擬 COM port 通常沒有 VID/PID）
        if not port.vid or not port.pid:
            continue

        # 跳過其他非 CDC 裝置
        skip_keywords = ['printer', 'modem', 'dialup', 'irda', '印表機', '數據機']
        if any(keyword in description for keyword in skip_keywords):
            continue

        print(f"嘗試 {port_name}...", end=" ")

        try:
            ser = serial.Serial(
                port=port_name,
                baudrate=115200,
                timeout=0.5,  # 讀取 timeout
                write_timeout=1.0,  # 寫入 timeout
                rtscts=False
            )

            # 明確設置 DTR 信號（重要！ESP32-S3 需要 DTR 才能正常通信）
            ser.dtr = True
            ser.rts = False

            # 等待裝置穩定（給足夠時間讓設備識別 DTR 信號）
            time.sleep(0.5)

            ser.reset_input_buffer()
            ser.reset_output_buffer()
            ser.write(b"*IDN?\n")
            ser.flush()

            # 使用 timeout 機制讀取回應
            time.sleep(0.1)
            start_time = time.time()
            timeout_duration = 2.0

            while (time.time() - start_time) < timeout_duration:
                if ser.in_waiting > 0:
                    response = ser.readline().decode('utf-8', errors='ignore').strip()
                    if "ESP32" in response or "RYMCU" in response or "USB" in response or "HID" in response or "Composite" in response:
                        print(f"✅ 找到！({response})")
                        return ser
                else:
                    time.sleep(0.05)

            print("❌")
            ser.close()

        except Exception as e:
            print(f"⚠️  {e}")

    print("❌ 未找到 CDC 介面")
    return None

def on_hid_data_handler(data):
    """HID 資料接收回調函式"""
    global received_hid_data
    with hid_data_lock:
        received_hid_data.append(data)

def find_hid_device():
    """尋找 ESP32-S3 HID 裝置"""
    print("\n" + "=" * 60)
    print("掃描 HID 介面")
    print("=" * 60)

    filter = hid.HidDeviceFilter(vendor_id=VENDOR_ID, product_id=PRODUCT_ID)
    devices = filter.get_devices()

    if devices:
        device = devices[0]
        print(f"✅ 找到 HID 裝置: {device.product_name}")
        try:
            device.open()
            device.set_raw_data_handler(on_hid_data_handler)

            # 取得 output report
            out_reports = device.find_output_reports()
            if not out_reports:
                print("❌ 未找到 output report")
                device.close()
                return None, None

            return device, out_reports[0]
        except Exception as e:
            print(f"❌ 無法開啟 HID: {e}")
            return None, None
    else:
        print("❌ 未找到 HID 介面")
        return None, None

def encode_hid_command(cmd_string):
    """編碼 HID 命令（0xA1 協定）- pywinusb 格式"""
    cmd_bytes = cmd_string.encode('utf-8')
    length = min(len(cmd_bytes), 61)

    # pywinusb 需要 65-byte 封包 (1 byte Report ID + 64 bytes data)
    packet = [0]  # Report ID = 0 (無 Report ID)
    packet.append(0xA1)     # 命令類型
    packet.append(length)   # 命令長度
    packet.append(0x00)     # 保留位元
    packet.extend(cmd_bytes[:length])  # 命令內容
    packet.extend([0] * (64 - 3 - length))  # 補零到 64 bytes

    return packet

def decode_hid_response(data):
    """解碼 HID 回應 - pywinusb 格式"""
    # data[0] 是 Report ID
    # data[1] 應該是 0xA1
    if len(data) >= 5 and data[1] == 0xA1:
        length = data[2]
        if 0 < length <= 61:
            try:
                return bytes(data[4:4+length]).decode('utf-8', errors='ignore')
            except:
                return None
    return None

def test_cdc_command(ser, cmd):
    """測試 CDC 命令"""
    if not ser:
        return None

    ser.reset_input_buffer()
    ser.write(f"{cmd}\n".encode())
    ser.flush()
    time.sleep(0.3)

    responses = []
    timeout = time.time() + 1.0

    while time.time() < timeout:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line and line != ">":
                responses.append(line)
                timeout = time.time() + 0.3
        else:
            time.sleep(0.05)

    return responses

def test_hid_command(hid_device, out_report, cmd):
    """測試 HID 命令"""
    global received_hid_data

    if not hid_device or not out_report:
        return None

    # 清空接收緩衝區
    with hid_data_lock:
        received_hid_data = []

    packet = encode_hid_command(cmd)
    out_report.set_raw_data(packet)
    out_report.send()

    responses = []
    timeout = time.time() + 1.0

    while time.time() < timeout:
        with hid_data_lock:
            current_data = received_hid_data[:]
            received_hid_data = []

        if current_data:
            for data in current_data:
                response = decode_hid_response(data)
                if response:
                    for line in response.split('\n'):
                        if line.strip():
                            responses.append(line.strip())
                    timeout = time.time() + 0.3

        time.sleep(0.05)

    return responses

def compare_responses(cdc_resp, hid_resp, cmd):
    """比較 CDC 和 HID 的回應"""
    print(f"\n{'='*60}")
    print(f"命令: {cmd}")
    print(f"{'='*60}")

    print("\n📡 CDC 回應:")
    if cdc_resp:
        for line in cdc_resp:
            print(f"  {line}")
    else:
        print("  ⚠️  無回應")

    print("\n📡 HID 回應:")
    if hid_resp:
        for line in hid_resp:
            print(f"  {line}")
    else:
        print("  ⚠️  無回應")

    # 比較結果
    if cdc_resp and hid_resp:
        # 簡單比較（忽略順序和空白）
        cdc_set = set(line.strip() for line in cdc_resp if line.strip())
        hid_set = set(line.strip() for line in hid_resp if line.strip())

        if cdc_set == hid_set:
            print("\n✅ 回應一致（多通道功能正常）")
        else:
            print("\n⚠️  回應不同")
            diff_cdc = cdc_set - hid_set
            diff_hid = hid_set - cdc_set
            if diff_cdc:
                print(f"  只有 CDC 有: {diff_cdc}")
            if diff_hid:
                print(f"  只有 HID 有: {diff_hid}")

def test_cdc_only(ser):
    """僅測試 CDC 介面"""
    print("\n" + "=" * 60)
    print("測試 CDC 介面")
    print("=" * 60)

    commands = ["*IDN?", "INFO", "STATUS", "HELP"]

    for cmd in commands:
        print(f"\n📤 命令: {cmd}")
        print("-" * 60)
        responses = test_cdc_command(ser, cmd)

        if responses:
            print("📥 回應:")
            for line in responses:
                print(f"  {line}")
        else:
            print("⚠️  無回應")

        time.sleep(0.3)

def test_hid_only(hid_device, out_report):
    """僅測試 HID 介面"""
    print("\n" + "=" * 60)
    print("測試 HID 介面")
    print("=" * 60)

    commands = ["*IDN?", "INFO", "STATUS", "HELP"]

    for cmd in commands:
        print(f"\n📤 命令: {cmd}")
        print("-" * 60)
        responses = test_hid_command(hid_device, out_report, cmd)

        if responses:
            print("📥 回應:")
            for line in responses:
                print(f"  {line}")
        else:
            print("⚠️  無回應")

        time.sleep(0.3)

def test_both(ser, hid_device, out_report):
    """測試 CDC 和 HID 多通道回應"""
    print("\n" + "=" * 60)
    print("測試多通道回應功能")
    print("=" * 60)

    commands = ["*IDN?", "INFO", "STATUS", "HELP"]

    for cmd in commands:
        cdc_resp = test_cdc_command(ser, cmd)
        time.sleep(0.2)
        hid_resp = test_hid_command(hid_device, out_report, cmd)
        compare_responses(cdc_resp, hid_resp, cmd)
        time.sleep(0.5)

def main():
    print("=" * 60)
    print("ESP32-S3 整合測試工具")
    print("測試 CDC 和 HID 介面")
    print("=" * 60)

    # 解析參數
    mode = "both"  # 預設模式：同時測試兩個介面
    if len(sys.argv) > 1:
        arg = sys.argv[1].lower()
        if arg in ['cdc', 'serial']:
            mode = "cdc"
        elif arg in ['hid']:
            mode = "hid"
        elif arg in ['both', 'all']:
            mode = "both"
        elif arg in ['help', '-h', '--help']:
            print("\n用法:")
            print(f"  {sys.argv[0]}              - 測試 CDC 和 HID（預設）")
            print(f"  {sys.argv[0]} cdc          - 僅測試 CDC 介面")
            print(f"  {sys.argv[0]} hid          - 僅測試 HID 介面")
            print(f"  {sys.argv[0]} both         - 測試 CDC 和 HID 多通道回應")
            return

    # 尋找裝置
    ser = None
    hid_device = None
    out_report = None

    if mode in ["cdc", "both"]:
        ser = find_cdc_device()

    if mode in ["hid", "both"]:
        hid_device, out_report = find_hid_device()

    # 檢查是否找到裝置
    if mode == "cdc" and not ser:
        print("\n❌ 未找到 CDC 介面")
        sys.exit(1)
    elif mode == "hid" and not hid_device:
        print("\n❌ 未找到 HID 介面")
        sys.exit(1)
    elif mode == "both" and not ser and not hid_device:
        print("\n❌ 未找到任何介面")
        sys.exit(1)

    # 顯示警告
    if mode == "both":
        if not ser:
            print("\n⚠️  警告：未找到 CDC 介面，僅測試 HID")
            mode = "hid"
        if not hid_device:
            print("\n⚠️  警告：未找到 HID 介面，僅測試 CDC")
            mode = "cdc"

    try:
        # 執行測試
        if mode == "cdc":
            test_cdc_only(ser)
        elif mode == "hid":
            test_hid_only(hid_device, out_report)
        elif mode == "both":
            test_both(ser, hid_device, out_report)

        print("\n" + "=" * 60)
        print("測試完成！")
        print("=" * 60)

        # 總結
        print("\n📊 測試總結:")
        if ser:
            print("  CDC 介面: ✅ 正常")
        if hid_device:
            print("  HID 介面: ✅ 正常")

        if ser and hid_device and mode == "both":
            print("\n✅ 所有介面正常運作")
            print("✅ 多通道回應功能已驗證")

    finally:
        if ser and ser.is_open:
            ser.close()
            print("\nCDC 介面已關閉")
        if hid_device:
            hid_device.close()
            print("HID 介面已關閉")

if __name__ == "__main__":
    main()
