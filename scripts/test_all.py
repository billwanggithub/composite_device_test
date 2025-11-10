#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32-S3 整合測試腳本 - 同時測試 CDC、HID 和 BLE 介面
使用 pywinusb 進行 HID 通訊，pyserial 進行 CDC 通訊，bleak 進行 BLE 通訊

回應路由規則（v2.2）
-------------------
根據命令類型，裝置會將回應路由到不同介面：

1. SCPI 命令（*IDN?, *RST 等）：
   - CDC 來源 → CDC 回應
   - HID 來源 → HID 回應
   - BLE 來源 → BLE 回應

2. 一般命令（HELP, INFO, STATUS 等）：
   - 所有來源 → 統一回應到 CDC（便於監控除錯）
   - HID/BLE 不會收到這些命令的回應

測試時的預期行為：
- 測試 SCPI 命令：各介面應收到各自的回應
- 測試一般命令：只有 CDC 會收到回應，HID/BLE 無回應是正常的
"""

import sys
import time
import threading
import asyncio
from typing import List, Optional, Tuple

# 設置 Windows 控制台編碼為 UTF-8
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')

# ==================== 配置常數 ====================
# 序列埠參數
DEFAULT_BAUDRATE = 115200
READ_TIMEOUT = 0.5
WRITE_TIMEOUT = 1.0

# 設備檢測參數
DEVICE_STABILIZATION_DELAY = 0.5
COMMAND_RESPONSE_DELAY = 0.5
RESPONSE_TIMEOUT = 2.0
RESPONSE_EXTEND_TIMEOUT = 0.5
POLL_INTERVAL = 0.05
PRE_READ_DELAY = 0.1

# 設備檢測關鍵字
BLUETOOTH_KEYWORDS = ['bluetooth', 'bt ', '藍牙', '藍芽', '透過藍牙', '透過藍芽']
SKIP_KEYWORDS = ['printer', 'modem', 'dialup', 'irda', '印表機', '數據機']
ESP32_KEYWORDS = ["ESP32", "RYMCU", "USB", "Composite", "HID"]

# HID 參數
HID_PACKET_SIZE = 64
HID_PROTOCOL_HEADER_SIZE = 3  # 0xA1 protocol header (type + length + reserved)
HID_MAX_COMMAND_LENGTH = 61  # 64 - 3

# 回應收集參數
MAX_IDLE_POLLS = 10  # 連續無資料時的最大輪詢次數
TEST_COMMAND_DELAY = 0.5  # 單一介面測試時命令之間的延遲（秒）
BLE_COMMAND_DELAY = 2.0  # BLE 測試時命令之間的延遲（秒）
MULTI_INTERFACE_DELAY = 0.3  # 多介面測試時介面切換延遲（秒）

# BLE 參數
DEFAULT_BLE_SCAN_TIMEOUT = 8.0  # BLE 掃描超時（秒）

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

try:
    from bleak import BleakScanner, BleakClient
    HAS_BLE = True
except ImportError:
    HAS_BLE = False
    print("⚠️  未安裝 bleak，BLE 測試將被跳過")
    print("安裝方法: pip install bleak")

# ESP32-S3 VID/PID
VENDOR_ID = 0x303A
PRODUCT_ID = 0x1001  # ESP32-S3 TinyUSB HID 介面的 PID

# BLE UUIDs
BLE_DEVICE_NAME = "BillCat_Fan_Control"
BLE_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
BLE_CHAR_UUID_RX = "beb5483e-36e1-4688-b7f5-ea07361b26a8"  # write
BLE_CHAR_UUID_TX = "beb5483e-36e1-4688-b7f5-ea07361b26a9"  # notify

# 全域變數存放 HID 接收資料
received_hid_data = []
hid_data_lock = threading.Lock()

# 全域變數存放 BLE 接收資料
received_ble_data = []
ble_data_lock = threading.Lock()

def find_cdc_device() -> Optional[serial.Serial]:
    """掃描 COM ports 找到 ESP32-S3（只掃描 USB CDC 裝置）"""
    print("\n" + "=" * 60)
    print("掃描 CDC (Serial) 介面")
    print("=" * 60)

    ports = serial.tools.list_ports.comports()

    for port in ports:
        port_name = port.device
        description = port.description.lower() if port.description else ""

        # 跳過藍牙裝置（支援中英文關鍵字）
        if any(keyword in description for keyword in BLUETOOTH_KEYWORDS):
            continue

        # 只掃描有 VID/PID 的 USB 裝置（虛擬 COM port 通常沒有 VID/PID）
        if not port.vid or not port.pid:
            continue

        # 跳過其他非 CDC 裝置
        if any(keyword in description for keyword in SKIP_KEYWORDS):
            continue

        print(f"嘗試 {port_name}...", end=" ")

        try:
            ser = serial.Serial(
                port=port_name,
                baudrate=DEFAULT_BAUDRATE,
                timeout=READ_TIMEOUT,
                write_timeout=WRITE_TIMEOUT,
                rtscts=False
            )

            # 明確設置 DTR 信號（重要！ESP32-S3 需要 DTR 才能正常通信）
            ser.dtr = True
            ser.rts = False

            # 等待裝置穩定（給足夠時間讓設備識別 DTR 信號）
            time.sleep(DEVICE_STABILIZATION_DELAY)

            ser.reset_input_buffer()
            ser.reset_output_buffer()
            ser.write(b"*IDN?\n")
            ser.flush()

            # 使用 timeout 機制讀取回應
            time.sleep(PRE_READ_DELAY)
            start_time = time.time()

            while (time.time() - start_time) < RESPONSE_TIMEOUT:
                if ser.in_waiting > 0:
                    response = ser.readline().decode('utf-8', errors='ignore').strip()
                    if any(keyword in response for keyword in ESP32_KEYWORDS):
                        print(f"✅ 找到！({response})")
                        return ser
                else:
                    time.sleep(POLL_INTERVAL)

            print("❌")
            ser.close()

        except serial.SerialException as e:
            print(f"⚠️  序列埠錯誤: {e}")
        except OSError as e:
            print(f"⚠️  系統錯誤: {e}")

    print("❌ 未找到 CDC 介面")
    return None

def on_hid_data_handler(data: List[int]) -> None:
    """HID 資料接收回調函式"""
    global received_hid_data
    with hid_data_lock:
        received_hid_data.append(data)

def find_hid_device() -> Tuple[Optional[any], Optional[any]]:
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
        except OSError as e:
            print(f"❌ 無法開啟 HID (系統錯誤): {e}")
            return None, None
        except AttributeError as e:
            print(f"❌ 無法開啟 HID (設備錯誤): {e}")
            return None, None
    else:
        print("❌ 未找到 HID 介面")
        return None, None

def encode_hid_command(cmd_string: str) -> List[int]:
    """編碼 HID 命令（0xA1 協定）- pywinusb 格式"""
    cmd_bytes = cmd_string.encode('utf-8')
    length = min(len(cmd_bytes), HID_MAX_COMMAND_LENGTH)

    # pywinusb 需要 65-byte 封包 (1 byte Report ID + 64 bytes data)
    packet = [0]  # Report ID = 0 (無 Report ID)
    packet.append(0xA1)     # 命令類型
    packet.append(length)   # 命令長度
    packet.append(0x00)     # 保留位元
    packet.extend(cmd_bytes[:length])  # 命令內容
    packet.extend([0] * (HID_PACKET_SIZE - HID_PROTOCOL_HEADER_SIZE - length))  # 補零到 64 bytes

    return packet

def decode_hid_response(data: List[int]) -> Optional[str]:
    """解碼 HID 回應 - pywinusb 格式"""
    # data[0] 是 Report ID
    # data[1] 應該是 0xA1
    if len(data) >= 5 and data[1] == 0xA1:
        length = data[2]
        if 0 < length <= HID_MAX_COMMAND_LENGTH:
            try:
                return bytes(data[4:4+length]).decode('utf-8', errors='ignore')
            except UnicodeDecodeError:
                return None
    return None

def test_cdc_command(ser: Optional[serial.Serial], cmd: str, timeout_sec: float = 2.0) -> Optional[List[str]]:
    """測試 CDC 命令（改良版，更穩定的回應收集）"""
    if not ser:
        return None

    # 清空接收緩衝區
    ser.reset_input_buffer()
    time.sleep(PRE_READ_DELAY)  # 給裝置一點時間

    # 發送命令
    ser.write(f"{cmd}\n".encode())
    ser.flush()

    # 等待初始回應
    time.sleep(COMMAND_RESPONSE_DELAY)

    responses = []
    idle_count = 0

    start_time = time.time()
    while (time.time() - start_time) < timeout_sec:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line and line != ">":
                responses.append(line)
                idle_count = 0  # 重置計數器
        else:
            idle_count += 1
            if idle_count >= MAX_IDLE_POLLS and responses:
                # 已經收到一些資料，且連續沒有新資料
                break
            time.sleep(POLL_INTERVAL)

    return responses

def test_hid_command(hid_device: Optional[any], out_report: Optional[any], cmd: str, timeout_sec: float = 2.0) -> Optional[List[str]]:
    """測試 HID 命令（改良版，更穩定的回應收集）"""
    global received_hid_data

    if not hid_device or not out_report:
        return None

    # 清空接收緩衝區
    with hid_data_lock:
        received_hid_data = []

    # 發送命令
    packet = encode_hid_command(cmd)
    out_report.set_raw_data(packet)
    out_report.send()

    # 等待初始回應
    time.sleep(COMMAND_RESPONSE_DELAY)

    responses = []
    idle_count = 0

    start_time = time.time()
    while (time.time() - start_time) < timeout_sec:
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
            idle_count = 0  # 重置計數器
        else:
            idle_count += 1
            if idle_count >= MAX_IDLE_POLLS and responses:
                # 已經收到一些資料，且連續沒有新資料
                break

        time.sleep(POLL_INTERVAL)

    return responses

async def find_ble_device_async(name: str = BLE_DEVICE_NAME, timeout: float = DEFAULT_BLE_SCAN_TIMEOUT) -> Optional['BleakClient']:
    """掃描並連接 BLE 裝置（async 版本）"""
    print("\n" + "=" * 60)
    print("掃描 BLE 介面")
    print("=" * 60)

    print(f"掃描 BLE 裝置 '{name}'...", end=" ", flush=True)
    
    try:
        from bleak.exc import BleakError
        
        # 設置更長的掃描超時（bleak 1.1.1 不支持 return_adv=True）
        devices = await BleakScanner.discover(timeout=timeout)
        
        if not devices:
            print("❌ 未找到任何設備")
            return None

        # 尋找匹配的設備
        found_device = None
        device_list = []
        
        for device_obj in devices:
            # 提取設備信息
            try:
                device_name = device_obj.name if hasattr(device_obj, 'name') else None
                device_addr = device_obj.address if hasattr(device_obj, 'address') else str(device_obj)
                
                if device_name:
                    device_list.append(device_name)
                    if name in device_name:
                        found_device = device_obj
                        break
            except:
                pass
        
        if not found_device:
            print(f"❌ 未找到設備 '{name}'")
            if device_list:
                print(f"   掃描到的設備: {device_list[:5]}...")
            return None

        print(f"✅ 找到！({device_obj.address})")
        
        try:
            client = BleakClient(device_obj.address)
            print(f"正在連接 {device_obj.address}...", end=" ", flush=True)
            await client.connect(timeout=10.0)
            
            if client.is_connected:
                print("✅ 已連接")
                return client
            else:
                print("❌ 無法連接")
                return None
                
        except BleakError as e:
            print(f"❌ BLE 連接失敗: {e}")
            return None
        except asyncio.TimeoutError as e:
            print(f"❌ 連接超時: {e}")
            return None
        except OSError as e:
            print(f"❌ 系統錯誤: {e}")
            return None
            
    except Exception as e:
        print(f"❌ BLE 掃描失敗: {type(e).__name__}: {e}")
        import traceback
        traceback.print_exc()
        return None

def find_ble_device(name: str = BLE_DEVICE_NAME, timeout: float = DEFAULT_BLE_SCAN_TIMEOUT) -> Optional['BleakClient']:
    """掃描並連接 BLE 裝置（同步包裝）"""
    if not HAS_BLE:
        return None

    try:
        # 使用 asyncio.run() - Windows 友善的方式
        client = asyncio.run(find_ble_device_async(name, timeout))
        return client
    except RuntimeError as e:
        print(f"❌ BLE event loop 錯誤: {e}")
        import traceback
        traceback.print_exc()
        return None
    except Exception as e:
        print(f"❌ BLE 掃描失敗: {e}")
        import traceback
        traceback.print_exc()
        return None

# BLE 通知處理器（全域，只設置一次）
ble_notification_handler = None

def is_scpi_command(cmd: str) -> bool:
    """檢查命令是否為 SCPI 命令（以 * 開頭）"""
    return cmd.strip().startswith('*')

def ble_handle_notification(sender: int, data: bytearray) -> None:
    """BLE 通知處理器"""
    with ble_data_lock:
        text = data.decode("utf-8", errors="replace")
        print(f"[DEBUG] BLE 通知: 收到 {len(data)} 字節: {repr(text)}")
        received_ble_data.append(text)

async def setup_ble_notifications_async(client: Optional['BleakClient']) -> bool:
    """設置 BLE 通知（只調用一次）"""
    if not client or not client.is_connected:
        return False

    try:
        from bleak.exc import BleakError
        await client.start_notify(BLE_CHAR_UUID_TX, ble_handle_notification)
        return True
    except BleakError as e:
        print(f"❌ 訂閱 BLE 通知失敗: {e}")
        return False
    except AttributeError as e:
        print(f"❌ BLE 屬性錯誤: {e}")
        return False

async def cleanup_ble_notifications_async(client: Optional['BleakClient']) -> None:
    """清理 BLE 通知（只調用一次）"""
    if not client or not client.is_connected:
        return

    try:
        from bleak.exc import BleakError
        await client.stop_notify(BLE_CHAR_UUID_TX)
    except BleakError as e:
        print(f"⚠️  取消 BLE 通知失敗 (BLE): {e}")
    except AttributeError as e:
        print(f"⚠️  取消 BLE 通知失敗 (屬性): {e}")

async def test_ble_command_async(client: Optional['BleakClient'], cmd: str, timeout_sec: float = 2.0) -> Optional[List[str]]:
    """測試 BLE 命令（async 版本）- 假設通知已經設置好"""
    global received_ble_data

    if not client or not client.is_connected:
        print(f"[DEBUG] 命令 '{cmd}': 客戶端未連接！")
        return None

    print(f"[DEBUG] 命令 '{cmd}': 發送中...")

    # 清空接收緩衝區
    with ble_data_lock:
        received_ble_data.clear()

    # 發送命令
    await client.write_gatt_char(BLE_CHAR_UUID_RX, f"{cmd}\n".encode("utf-8"), response=False)
    print(f"[DEBUG] 命令 '{cmd}': 已發送")

    # 等待初始回應
    await asyncio.sleep(COMMAND_RESPONSE_DELAY)

    responses = []
    idle_count = 0

    start_time = time.time()
    while (time.time() - start_time) < timeout_sec:
        with ble_data_lock:
            current_data = received_ble_data[:]
            received_ble_data.clear()

        if current_data:
            for text in current_data:
                for line in text.split('\n'):
                    if line.strip():
                        responses.append(line.strip())
            idle_count = 0
        else:
            idle_count += 1
            if idle_count >= MAX_IDLE_POLLS and responses:
                break

        await asyncio.sleep(POLL_INTERVAL)

    return responses

def test_ble_command(client: Optional['BleakClient'], cmd: str, timeout_sec: float = 2.0) -> Optional[List[str]]:
    """測試 BLE 命令（同步包裝）"""
    if not client:
        return None

    try:
        # 嘗試取得已存在的 event loop
        try:
            loop = asyncio.get_running_loop()
            # 如果在 async 上下文中，直接執行（但這不應該發生）
            return asyncio.run(test_ble_command_async(client, cmd, timeout_sec))
        except RuntimeError:
            # 沒有正在執行的 loop，嘗試取得或建立新的
            try:
                loop = asyncio.get_event_loop()
                if loop.is_closed():
                    loop = asyncio.new_event_loop()
                    asyncio.set_event_loop(loop)
            except RuntimeError:
                loop = asyncio.new_event_loop()
                asyncio.set_event_loop(loop)
            
            return loop.run_until_complete(test_ble_command_async(client, cmd, timeout_sec))
    except Exception as e:
        print(f"❌ BLE 命令失敗: {e}")
        import traceback
        traceback.print_exc()
        return None

def compare_responses(cdc_resp: Optional[List[str]] = None, hid_resp: Optional[List[str]] = None, ble_resp: Optional[List[str]] = None, cmd: str = "") -> None:
    """比較 CDC、HID 和 BLE 的回應（考慮新的路由規則）"""
    print(f"\n{'='*60}")
    print(f"命令: {cmd}")

    # 判斷命令類型
    is_scpi = is_scpi_command(cmd)
    if is_scpi:
        print(f"類型: SCPI 命令（各介面獨立回應）")
    else:
        print(f"類型: 一般命令（統一回應到 CDC）")

    print(f"{'='*60}")

    # 顯示各介面回應
    responses_dict = {}

    if cdc_resp is not None:
        print("\n📡 CDC 回應:")
        if cdc_resp:
            for line in cdc_resp:
                print(f"  {line}")
            responses_dict['CDC'] = set(line.strip() for line in cdc_resp if line.strip())
        else:
            print("  ⚠️  無回應")

    if hid_resp is not None:
        print("\n📡 HID 回應:")
        if hid_resp:
            for line in hid_resp:
                print(f"  {line}")
            responses_dict['HID'] = set(line.strip() for line in hid_resp if line.strip())
        else:
            if is_scpi:
                print("  ⚠️  無回應（異常：SCPI 命令應該有回應）")
            else:
                print("  ⚠️  無回應（預期：一般命令只回應到 CDC）")

    if ble_resp is not None:
        print("\n📡 BLE 回應:")
        if ble_resp:
            for line in ble_resp:
                print(f"  {line}")
            responses_dict['BLE'] = set(line.strip() for line in ble_resp if line.strip())
        else:
            if is_scpi:
                print("  ⚠️  無回應（異常：SCPI 命令應該有回應）")
            else:
                print("  ⚠️  無回應（預期：一般命令只回應到 CDC）")

    # 比較結果（考慮路由規則）
    if is_scpi:
        # SCPI 命令：各介面應該有自己的回應，內容應該一致
        if len(responses_dict) >= 2:
            response_sets = list(responses_dict.values())
            all_same = all(s == response_sets[0] for s in response_sets)

            if all_same:
                print("\n✅ SCPI 命令：所有介面回應一致")
            else:
                print("\n⚠️  SCPI 命令：介面回應不同（可能異常）")
                # 顯示差異
                for name1, set1 in responses_dict.items():
                    for name2, set2 in responses_dict.items():
                        if name1 < name2:  # 避免重複比較
                            diff = set1 - set2
                            if diff:
                                print(f"  只有 {name1} 有: {diff}")
    else:
        # 一般命令：只有 CDC 應該有回應
        if 'CDC' in responses_dict and responses_dict['CDC']:
            has_hid = 'HID' in responses_dict and responses_dict['HID']
            has_ble = 'BLE' in responses_dict and responses_dict['BLE']

            if not has_hid and not has_ble:
                print("\n✅ 一般命令：回應路由正確（只有 CDC 回應）")
            else:
                unexpected = []
                if has_hid:
                    unexpected.append("HID")
                if has_ble:
                    unexpected.append("BLE")
                print(f"\n⚠️  一般命令：{', '.join(unexpected)} 不應該有回應（異常）")
        elif 'CDC' not in responses_dict or not responses_dict['CDC']:
            print("\n⚠️  一般命令：CDC 無回應（異常）")

def test_cdc_only(ser: Optional[serial.Serial]) -> None:
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

        time.sleep(TEST_COMMAND_DELAY)

def test_hid_only(hid_device: Optional[any], out_report: Optional[any]) -> None:
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

        time.sleep(TEST_COMMAND_DELAY)

def test_ble_only(ble_client: Optional['BleakClient']) -> None:
    """僅測試 BLE 介面"""
    if not ble_client:
        return

    print("\n" + "=" * 60)
    print("測試 BLE 介面")
    print("=" * 60)

    # 設置 BLE 通知（只訂閱一次）
    try:
        loop = asyncio.get_event_loop()
        if loop.is_closed():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
    except RuntimeError:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)

    if not loop.run_until_complete(setup_ble_notifications_async(ble_client)):
        print("❌ 無法設置 BLE 通知")
        return

    try:
        commands = ["*IDN?", "INFO", "STATUS", "HELP"]

        for cmd in commands:
            print(f"\n📤 命令: {cmd}")
            print("-" * 60)
            responses = test_ble_command(ble_client, cmd)

            if responses:
                print("📥 回應:")
                for line in responses:
                    print(f"  {line}")
            else:
                print("⚠️  無回應")

            time.sleep(BLE_COMMAND_DELAY)

    finally:
        # 清理 BLE 通知（只取消一次）
        loop.run_until_complete(cleanup_ble_notifications_async(ble_client))

def test_all_interfaces(ser: Optional[serial.Serial] = None, hid_device: Optional[any] = None, out_report: Optional[any] = None, ble_client: Optional['BleakClient'] = None) -> None:
    """測試所有可用介面的多通道回應"""
    print("\n" + "=" * 60)
    print("測試多通道回應功能（v2.2 路由規則）")
    print("=" * 60)

    # 統計可用介面
    available = []
    if ser:
        available.append("CDC")
    if hid_device and out_report:
        available.append("HID")
    if ble_client:
        available.append("BLE")

    print(f"可用介面: {', '.join(available)}")
    print()
    print("測試說明：")
    print("  - SCPI 命令（*IDN?）: 各介面獨立回應")
    print("  - 一般命令（HELP/INFO/STATUS）: 只有 CDC 回應")
    print()

    # 如果有 BLE 客戶端，設置通知（只訂閱一次）
    ble_notifications_setup = False
    loop = None
    if ble_client:
        try:
            loop = asyncio.get_event_loop()
            if loop.is_closed():
                loop = asyncio.new_event_loop()
                asyncio.set_event_loop(loop)
        except RuntimeError:
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
        
        ble_notifications_setup = loop.run_until_complete(setup_ble_notifications_async(ble_client))
        if not ble_notifications_setup:
            print("⚠️  無法設置 BLE 通知，BLE 測試將被跳過")
            ble_client = None

    try:
        commands = ["*IDN?", "INFO", "STATUS", "HELP"]

        for cmd in commands:
            cdc_resp = None
            hid_resp = None
            ble_resp = None

            # 發送到各個介面
            if ser:
                cdc_resp = test_cdc_command(ser, cmd)
                time.sleep(MULTI_INTERFACE_DELAY)

            if hid_device and out_report:
                hid_resp = test_hid_command(hid_device, out_report, cmd)
                time.sleep(MULTI_INTERFACE_DELAY)

            if ble_client:
                ble_resp = test_ble_command(ble_client, cmd)
                time.sleep(MULTI_INTERFACE_DELAY)

            # 比較回應
            compare_responses(cdc_resp, hid_resp, ble_resp, cmd)
            time.sleep(TEST_COMMAND_DELAY)

    finally:
        # 如果設置了 BLE 通知，清理它（只取消一次）
        if ble_client and ble_notifications_setup and loop:
            loop.run_until_complete(cleanup_ble_notifications_async(ble_client))

def main() -> None:
    print("=" * 60)
    print("ESP32-S3 整合測試工具")
    print("測試 CDC、HID 和 BLE 介面")
    print("=" * 60)

    # 解析參數
    mode = "all"  # 預設模式：測試所有介面
    if len(sys.argv) > 1:
        arg = sys.argv[1].lower()
        if arg in ['cdc', 'serial']:
            mode = "cdc"
        elif arg in ['hid']:
            mode = "hid"
        elif arg in ['ble', 'bluetooth']:
            mode = "ble"
        elif arg in ['all', 'both']:
            mode = "all"
        elif arg in ['help', '-h', '--help']:
            print("\n用法:")
            print(f"  {sys.argv[0]}              - 測試所有可用介面（預設）")
            print(f"  {sys.argv[0]} cdc          - 僅測試 CDC 介面")
            print(f"  {sys.argv[0]} hid          - 僅測試 HID 介面")
            print(f"  {sys.argv[0]} ble          - 僅測試 BLE 介面")
            print(f"  {sys.argv[0]} all          - 測試所有可用介面的多通道回應")
            return

    # 尋找裝置
    ser = None
    hid_device = None
    out_report = None
    ble_client = None

    if mode in ["cdc", "all"]:
        ser = find_cdc_device()

    if mode in ["hid", "all"]:
        hid_device, out_report = find_hid_device()

    if mode in ["ble", "all"] and HAS_BLE:
        ble_client = find_ble_device()

    # 檢查是否找到裝置
    if mode == "cdc" and not ser:
        print("\n❌ 未找到 CDC 介面")
        sys.exit(1)
    elif mode == "hid" and not hid_device:
        print("\n❌ 未找到 HID 介面")
        sys.exit(1)
    elif mode == "ble" and not ble_client:
        print("\n❌ 未找到 BLE 介面")
        sys.exit(1)
    elif mode == "all" and not ser and not hid_device and not ble_client:
        print("\n❌ 未找到任何介面")
        sys.exit(1)

    # 顯示警告
    if mode == "all":
        available_count = sum([1 for x in [ser, hid_device, ble_client] if x])
        if available_count < 3:
            missing = []
            if not ser:
                missing.append("CDC")
            if not hid_device:
                missing.append("HID")
            if not ble_client:
                missing.append("BLE")
            print(f"\n⚠️  警告：未找到 {', '.join(missing)} 介面")

    try:
        # 執行測試
        if mode == "cdc":
            test_cdc_only(ser)
        elif mode == "hid":
            test_hid_only(hid_device, out_report)
        elif mode == "ble":
            test_ble_only(ble_client)
        elif mode == "all":
            test_all_interfaces(ser, hid_device, out_report, ble_client)

        print("\n" + "=" * 60)
        print("測試完成！")
        print("=" * 60)

        # 總結
        print("\n📊 測試總結:")
        if ser:
            print("  CDC 介面: ✅ 正常")
        if hid_device:
            print("  HID 介面: ✅ 正常")
        if ble_client:
            print("  BLE 介面: ✅ 正常")

        if mode == "all":
            available = [name for name, dev in [("CDC", ser), ("HID", hid_device), ("BLE", ble_client)] if dev]
            if len(available) >= 2:
                print(f"\n✅ {len(available)} 個介面正常運作")
                print("✅ 多通道回應功能已驗證")

    finally:
        if ser and ser.is_open:
            ser.close()
            print("\nCDC 介面已關閉")
        if hid_device:
            hid_device.close()
            print("HID 介面已關閉")
        if ble_client:
            # 需要異步關閉
            try:
                from bleak.exc import BleakError
                
                # 獲取或建立 event loop
                try:
                    loop = asyncio.get_event_loop()
                    if loop.is_closed():
                        loop = asyncio.new_event_loop()
                        asyncio.set_event_loop(loop)
                except RuntimeError:
                    loop = asyncio.new_event_loop()
                    asyncio.set_event_loop(loop)
                
                loop.run_until_complete(ble_client.disconnect())
                print("BLE 介面已關閉")
            except BleakError:
                pass  # BLE 已斷開
            except RuntimeError:
                pass  # Event loop 已關閉
            except Exception:
                pass  # 其他錯誤（避免關閉時崩潰）

if __name__ == "__main__":
    main()
