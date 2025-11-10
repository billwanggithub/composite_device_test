#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
簡化版 BLE 測試腳本 - 使用 asyncio.run() 確保穩定性
直接在 async context 中運行所有操作，避免 event loop 複雜性
"""

import sys
import asyncio
import time
from typing import Optional

# 檢查依賴
try:
    from bleak import BleakScanner, BleakClient
    from bleak.exc import BleakError
except ImportError:
    print("❌ 需要安裝 bleak")
    print("請執行: pip install bleak")
    sys.exit(1)

# BLE 配置
BLE_DEVICE_NAME = "BillCat_Fan_Control"
BLE_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
BLE_CHAR_UUID_RX = "beb5483e-36e1-4688-b7f5-ea07361b26a8"  # Write
BLE_CHAR_UUID_TX = "beb5483e-36e1-4688-b7f5-ea07361b26a9"  # Notify

# 全域變數存放接收資料
received_data = []

def notification_handler(sender: int, data: bytearray) -> None:
    """通知處理器"""
    text = data.decode("utf-8", errors="replace")
    received_data.append(text)
    print(f"[通知] {text}")

async def scan_and_connect(device_name: str = BLE_DEVICE_NAME, timeout: float = 8.0) -> Optional[BleakClient]:
    """掃描並連接 BLE 設備"""
    print(f"\n掃描 BLE 設備 '{device_name}'...", flush=True)
    
    try:
        # 掃描設備
        devices = await BleakScanner.discover(timeout=timeout, return_adv=True)
        
        if not devices:
            print("❌ 未找到任何設備")
            return None
        
        # 列出掃描到的設備
        print(f"\n掃描到 {len(devices)} 個設備:")
        for device, adv_data in devices.items():
            if device.name:
                print(f"  - {device.name} ({device.address})")
        
        # 尋找目標設備
        target_device = None
        for device, adv_data in devices.items():
            if device.name and device_name in device.name:
                target_device = device
                break
        
        if not target_device:
            print(f"\n❌ 未找到設備 '{device_name}'")
            return None
        
        # 連接設備
        print(f"\n連接 {target_device.name} ({target_device.address})...", flush=True)
        client = BleakClient(target_device.address)
        
        try:
            await client.connect(timeout=10.0)
            print(f"✅ 已連接")
            
            # 訂閱通知
            print(f"訂閱 TX 特性通知...", flush=True)
            await client.start_notify(BLE_CHAR_UUID_TX, notification_handler)
            print(f"✅ 通知已訂閱")
            
            return client
            
        except asyncio.TimeoutError:
            print("❌ 連接超時")
            return None
        except BleakError as e:
            print(f"❌ BLE 錯誤: {e}")
            return None
    
    except Exception as e:
        print(f"❌ 掃描失敗: {type(e).__name__}: {e}")
        import traceback
        traceback.print_exc()
        return None

async def send_command(client: BleakClient, command: str) -> None:
    """發送命令"""
    global received_data
    
    print(f"\n📤 發送: {command}")
    received_data.clear()
    
    try:
        await client.write_gatt_char(BLE_CHAR_UUID_RX, f"{command}\n".encode("utf-8"))
        
        # 等待回應
        print("等待回應...", flush=True)
        await asyncio.sleep(1.0)
        
        if received_data:
            print(f"📥 收到 {len(received_data)} 條回應")
        else:
            print("❌ 無回應")
    
    except BleakError as e:
        print(f"❌ 發送失敗: {e}")
    except Exception as e:
        print(f"❌ 錯誤: {e}")

async def interactive_mode(client: BleakClient) -> None:
    """互動模式"""
    print("\n" + "=" * 60)
    print("BLE 互動模式")
    print("=" * 60)
    print("輸入命令（例如: HELP, INFO, STATUS, RPM）")
    print("輸入 'exit' 退出\n")
    
    while True:
        try:
            cmd = input("> ").strip()
            if not cmd:
                continue
            if cmd.lower() == 'exit':
                break
            
            await send_command(client, cmd)
            await asyncio.sleep(0.5)
        
        except KeyboardInterrupt:
            print("\n已中止")
            break
        except Exception as e:
            print(f"❌ 錯誤: {e}")

async def test_mode(client: BleakClient) -> None:
    """自動測試模式"""
    print("\n" + "=" * 60)
    print("BLE 自動測試")
    print("=" * 60)
    
    commands = ["*IDN?", "INFO", "STATUS"]
    
    for cmd in commands:
        await send_command(client, cmd)
        await asyncio.sleep(1.0)

async def main():
    """主函數"""
    print("=" * 60)
    print("BLE 簡化測試工具")
    print("=" * 60)
    
    # 掃描並連接
    client = await scan_and_connect()
    
    if not client:
        print("\n❌ 無法連接到設備")
        return
    
    try:
        # 選擇模式
        mode = "interactive"  # 預設互動模式
        if len(sys.argv) > 1:
            if sys.argv[1].lower() == "test":
                mode = "test"
        
        if mode == "test":
            await test_mode(client)
        else:
            await interactive_mode(client)
    
    finally:
        # 清理
        if client.is_connected:
            try:
                await client.stop_notify(BLE_CHAR_UUID_TX)
                await client.disconnect()
                print("\n✅ 已斷開連接")
            except BleakError:
                pass
            except Exception:
                pass

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n已中止")
    except Exception as e:
        print(f"\n❌ 致命錯誤: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
