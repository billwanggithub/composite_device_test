#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32-S3 USB CDC 測試工具
自動掃描並測試 CDC 序列埠介面
支援命令測試和互動模式
"""

import serial
import serial.tools.list_ports
import time
import sys

def list_ports_info():
    """列出所有 COM ports 的詳細資訊"""
    print("=" * 60)
    print("所有可用的 COM Ports:")
    print("=" * 60)
    ports = serial.tools.list_ports.comports()

    if not ports:
        print("未找到任何 COM port")
        return

    for port in ports:
        print(f"\nPort: {port.device}")
        print(f"  描述: {port.description}")
        print(f"  VID:PID: {port.vid:04X}:{port.pid:04X}" if port.vid and port.pid else "  VID:PID: N/A")
        print(f"  序號: {port.serial_number}" if port.serial_number else "  序號: N/A")

def find_esp32_device():
    """掃描所有 COM ports，使用 *IDN? 命令找到 ESP32-S3 裝置"""
    print("\n" + "=" * 60)
    print("掃描 COM Ports (只掃描 USB CDC 裝置)")
    print("=" * 60)

    ports = serial.tools.list_ports.comports()

    if not ports:
        print("❌ 未找到任何 COM port\n")
        return None

    for port in ports:
        port_name = port.device
        description = port.description.lower() if port.description else ""

        # 跳過藍牙裝置（支援中英文關鍵字）
        bluetooth_keywords = ['bluetooth', 'bt ', '藍牙', '藍芽', '透過藍牙', '透過藍芽']
        if any(keyword in description for keyword in bluetooth_keywords):
            print(f"⏭️  跳過 {port_name} (藍牙裝置)")
            continue

        # 只掃描有 VID/PID 的 USB 裝置（虛擬 COM port 通常沒有 VID/PID）
        if not port.vid or not port.pid:
            print(f"⏭️  跳過 {port_name} (虛擬 COM port，無 VID/PID)")
            continue

        # 跳過其他非 CDC 裝置
        skip_keywords = ['printer', 'modem', 'dialup', 'irda', '印表機', '數據機']
        if any(keyword in description for keyword in skip_keywords):
            print(f"⏭️  跳過 {port_name} (非 CDC 裝置)")
            continue

        # 嘗試連接所有 COM ports
        print(f"\n嘗試 {port_name}...")
        print(f"  描述: {port.description}")
        if port.vid and port.pid:
            print(f"  VID:PID: {port.vid:04X}:{port.pid:04X}")
        else:
            print(f"  VID:PID: N/A")

        try:
            # 嘗試開啟序列埠（115200 baud, DTR enabled, 設定 timeout）
            print(f"  開啟序列埠...")
            ser = serial.Serial(
                port=port_name,
                baudrate=115200,
                timeout=0.5,  # 縮短讀取 timeout
                write_timeout=1.0,  # 寫入 timeout
                rtscts=False
            )

            # 明確設置 DTR 信號（重要！ESP32-S3 需要 DTR 才能正常通信）
            ser.dtr = True
            ser.rts = False

            # 等待裝置穩定（給足夠時間讓設備識別 DTR 信號）
            print(f"  等待裝置穩定...")
            time.sleep(0.5)  # 縮短等待時間

            # 清除緩衝區
            ser.reset_input_buffer()
            ser.reset_output_buffer()

            # 發送 *IDN? 命令
            print(f"  發送命令: *IDN?")
            ser.write(b"*IDN?\n")
            ser.flush()

            # 等待回應（總計最多 2 秒）
            time.sleep(0.1)

            # 讀取回應
            response = ""
            start_time = time.time()
            timeout_duration = 2.0  # 總 timeout 時間

            while (time.time() - start_time) < timeout_duration:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        response += line + "\n"
                        # 檢查是否為 ESP32-S3
                        if "ESP32" in line or "RYMCU" in line or "USB" in line or "Composite" in line or "HID" in line:
                            print(f"  ✅ 找到裝置！回應: {line}")
                            return ser  # 保持連線開啟
                else:
                    time.sleep(0.05)  # 短暫等待，避免過度消耗 CPU

            # 沒有找到，關閉連線
            elapsed = time.time() - start_time
            if response:
                print(f"  回應: {response.strip()} (非目標裝置)")
            else:
                print(f"  ⏱️  無回應 (timeout {elapsed:.1f}s)")
            ser.close()

        except serial.SerialException as e:
            print(f"  ⚠️  無法開啟: {e}")
        except Exception as e:
            print(f"  ⚠️  錯誤: {e}")

    print("\n❌ 未找到 ESP32-S3 裝置\n")
    return None

def send_command(ser, cmd):
    """發送命令並返回回應"""
    # 清除輸入緩衝區，但保留 DTR 狀態
    ser.reset_input_buffer()

    # 發送命令
    ser.write(f"{cmd}\n".encode())
    ser.flush()

    # 等待設備處理命令
    time.sleep(0.5)

    response_lines = []
    timeout = time.time() + 2.0

    while time.time() < timeout:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line and line != ">":  # 忽略提示符
                response_lines.append(line)
                timeout = time.time() + 0.5  # 延長 timeout
        else:
            time.sleep(0.05)

    return response_lines

def test_commands(ser):
    """測試各種命令"""
    print("\n" + "="*60)
    print("開始測試命令")
    print("="*60)

    commands = [
        ("*IDN?", "裝置識別"),
        ("INFO", "裝置資訊"),
        ("STATUS", "系統狀態"),
        ("HELP", "命令列表"),
    ]

    for cmd, desc in commands:
        print(f"\n📤 測試命令: {cmd} ({desc})")
        print("-" * 60)

        response_lines = send_command(ser, cmd)

        # 顯示回應
        if response_lines:
            print("📥 回應:")
            for line in response_lines:
                print(f"  {line}")
        else:
            print("⚠️  無回應")

    print("\n" + "="*60)
    print("測試完成")
    print("="*60)

def interactive_mode(ser):
    """互動模式"""
    print("\n" + "="*60)
    print("進入互動模式")
    print("輸入命令並按 Enter，輸入 'quit' 或 'exit' 離開")
    print("="*60)

    try:
        while True:
            # 顯示提示符
            cmd = input("\n> ").strip()

            if cmd.lower() in ['quit', 'exit', 'q']:
                print("離開互動模式")
                break

            if not cmd:
                continue

            # 發送命令並顯示回應
            response_lines = send_command(ser, cmd)

            if response_lines:
                for line in response_lines:
                    print(line)
            else:
                print("(無回應)")

    except KeyboardInterrupt:
        print("\n\n中斷！")

def main():
    print("=" * 60)
    print("ESP32-S3 CDC Serial 測試工具")
    print("=" * 60)

    if len(sys.argv) > 1:
        command = sys.argv[1].lower()

        if command == 'list':
            list_ports_info()
            return
        elif command == 'help':
            print("\n用法:")
            print(f"  {sys.argv[0]}              - 自動掃描並測試")
            print(f"  {sys.argv[0]} list         - 列出所有 COM ports")
            print(f"  {sys.argv[0]} interactive  - 進入互動模式")
            print(f"  {sys.argv[0]} test         - 測試所有命令")
            return

    # 掃描並找到裝置
    ser = find_esp32_device()

    if not ser:
        print("請確認：")
        print("1. ESP32-S3 已連接到電腦")
        print("2. 韌體已正確上傳")
        print("3. USB 線已重新插拔（確保 USB 重新列舉）")
        sys.exit(1)

    try:
        # 根據參數決定行為
        if len(sys.argv) > 1:
            command = sys.argv[1].lower()
            if command == 'interactive' or command == 'i':
                interactive_mode(ser)
            elif command == 'test':
                test_commands(ser)
        else:
            # 預設：測試命令後進入互動模式
            test_commands(ser)

            choice = input("\n是否進入互動模式？ (y/n): ").strip().lower()
            if choice == 'y':
                interactive_mode(ser)

    finally:
        # 關閉序列埠
        if ser and ser.is_open:
            ser.close()
            print("\n序列埠已關閉")

if __name__ == "__main__":
    main()
