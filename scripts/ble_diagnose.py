#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BLE 診斷工具 - 檢查藍牙連接問題
"""

import sys
import os
import asyncio
import time

# 設置編碼
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')

def print_section(title):
    """打印章節標題"""
    print("\n" + "=" * 60)
    print(title)
    print("=" * 60)

def print_ok(msg):
    """打印成功消息"""
    print(f"[OK] {msg}")

def print_warn(msg):
    """打印警告消息"""
    print(f"[WARN] {msg}")

def print_fail(msg):
    """打印失敗消息"""
    print(f"[FAIL] {msg}")

def check_python():
    """檢查 Python 版本"""
    print_section("Python 環境檢查")
    
    version = f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"
    print(f"Python 版本: {version}")
    print(f"Platform: {sys.platform}")
    
    if sys.version_info >= (3, 10):
        print_ok("Python 版本符合要求 (>= 3.10)")
        return True
    else:
        print_warn("建議升級到 Python 3.10 或更高版本")
        return False

def check_bleak():
    """檢查 Bleak 安裝"""
    print_section("Bleak 依賴檢查")
    
    try:
        import bleak
        print_ok("Bleak 已安裝")
        return True
    except ImportError:
        print_fail("Bleak 未安裝")
        print("  執行: pip install --upgrade bleak")
        return False

async def test_ble_scan():
    """測試 BLE 掃描"""
    print_section("BLE 掃描測試")
    
    try:
        from bleak import BleakScanner
        
        print("正在掃描 BLE 設備 (5 秒超時)...")
        devices = await BleakScanner.discover(timeout=5, return_adv=True)
        
        if not devices:
            print_warn("掃描成功但未發現任何設備")
            return False
        
        print_ok(f"掃描成功，發現 {len(devices)} 個設備:")
        
        billcat_found = False
        for device in devices:
            try:
                # 處理不同的 Bleak 版本返回格式
                name = None
                address = None
                
                # Bleak 0.20+ 格式: (BLEDevice, AdvertisementData) 元組
                if isinstance(device, tuple) and len(device) == 2:
                    device_obj, adv_data = device
                    name = device_obj.name
                    address = device_obj.address
                    
                    # 如果 name 為空，嘗試從廣告數據中提取
                    if not name and hasattr(adv_data, 'local_name'):
                        name = adv_data.local_name
                
                # Bleak < 0.20 格式: BLEDevice 對象
                elif hasattr(device, 'name') and hasattr(device, 'address'):
                    name = device.name
                    address = device.address
                
                # 回退方案
                if not name:
                    name = "(無名稱)"
                if not address:
                    address = ""
                
                # 檢查是否找到目標設備
                if name and "BillCat" in name:
                    billcat_found = True
                    print(f"  * {name} ({address}) <-- BillCat_Fan_Control 🎯")
                elif name != "(無名稱)" and address:
                    print(f"  - {name} ({address})")
                else:
                    # 只顯示沒有名稱但有地址的設備
                    if address:
                        print(f"  - 未命名 ({address})")
            except Exception as e:
                print(f"  ? 解析失敗: {e}")
        
        if billcat_found:
            print_ok("BillCat_Fan_Control 已發現!")
        else:
            print_warn("未發現 BillCat_Fan_Control，但掃描成功")
        
        return billcat_found
    
    except OSError as e:
        print_fail(f"掃描失敗: {e}")
        print("\n故障排除:")
        print("  1. 檢查藍牙驅動是否正常")
        print("  2. 重新啟動藍牙服務 (PowerShell 管理員):")
        print("     Restart-Service -Name BluetoothUserService -Force")
        print("  3. 重新啟動電腦")
        return False
    
    except Exception as e:
        print_fail(f"意外錯誤: {type(e).__name__}: {e}")
        return False

async def main():
    """主程序"""
    print("\n" + "=" * 60)
    print("Windows BLE 診斷工具")
    print("=" * 60 + "\n")
    
    # 1. Python 檢查
    python_ok = check_python()
    
    # 2. Bleak 檢查
    bleak_ok = check_bleak()
    
    if not bleak_ok:
        print_section("診斷終止")
        print("Bleak 未安裝，無法繼續診斷")
        return
    
    print("\n[提示] 此診斷不會影響 CDC 連接")
    
    # 3. BLE 掃描測試
    scan_ok = await test_ble_scan()
    
    # 總結
    print_section("診斷結果")
    
    if scan_ok:
        print_ok("藍牙掃描正常")
        print("\n現在可以運行: python ble_simple.py")
    else:
        print_fail("藍牙掃描失敗")
        print("\n建議步驟:")
        print("  1. 驗證 ESP32 是否正在廣播 BLE")
        print("  2. 在手機上使用 nRF Connect 測試 BLE")
        print("  3. 如果手機能發現但電腦不能，說明是 Windows 驅動問題")
        print("  4. 改用 CDC (USB 序列埠) 或 HID 介面進行測試")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n已中止")
    except Exception as e:
        print_fail(f"致命錯誤: {e}")
        import traceback
        traceback.print_exc()
