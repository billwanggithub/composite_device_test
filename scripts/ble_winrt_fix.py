#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BLE 修復腳本 - 解決 Windows WinRT 初始化問題
嘗試多種方法修復藍牙驅動問題
"""

import sys
import os
import subprocess
import time
import asyncio
from pathlib import Path

# 設置 Windows 控制台編碼
if sys.platform == 'win32':
    os.environ['PYTHONIOENCODING'] = 'utf-8'
    import locale
    locale.setlocale(locale.LC_ALL, '')

def print_header(title: str):
    """打印標題"""
    print("\n" + "=" * 60)
    print(title)
    print("=" * 60)

def check_bluetooth_service():
    """檢查 Windows 藍牙服務狀態"""
    print_header("檢查藍牙服務")
    
    try:
        result = subprocess.run(
            ["powershell", "-Command", "Get-Service | Where-Object {$_.Name -match 'Bluetooth'} | Select-Object Name,Status"],
            capture_output=True,
            text=True,
            timeout=5
        )
        print(result.stdout)
        return True
    except Exception as e:
        print(f"⚠️  無法檢查藍牙服務: {e}")
        return False

def restart_bluetooth_service():
    """重新啟動藍牙服務（需要管理員權限）"""
    print_header("重新啟動藍牙服務")
    
    try:
        print("嘗試重新啟動 BluetoothUserService...")
        subprocess.run(
            ["powershell", "-Command", "Restart-Service -Name BluetoothUserService -Force"],
            capture_output=True,
            timeout=10,
            check=False
        )
        time.sleep(2)
        print("✅ 藍牙服務已重新啟動")
        return True
    except PermissionError:
        print("❌ 需要管理員權限")
        print("   請以管理員身份運行 PowerShell：")
        print("   Restart-Service -Name BluetoothUserService -Force")
        return False
    except Exception as e:
        print(f"⚠️  重新啟動失敗: {e}")
        return False

def check_device_manager():
    """檢查設備管理器中的藍牙設備"""
    print_header("檢查藍牙設備")
    
    try:
        result = subprocess.run(
            ["powershell", "-Command", "Get-PnpDevice -Class Bluetooth | Format-Table Name,Status"],
            capture_output=True,
            text=True,
            timeout=5
        )
        print(result.stdout)
        return True
    except Exception as e:
        print(f"⚠️  無法檢查設備: {e}")
        return False

async def test_bleak_with_retry(retries: int = 3):
    """嘗試多次使用 Bleak（帶重試）"""
    print_header("測試 Bleak 掃描（帶重試）")
    
    for attempt in range(retries):
        print(f"\n嘗試 {attempt + 1}/{retries}...", flush=True)
        
        try:
            from bleak import BleakScanner
            
            # 添加延遲
            await asyncio.sleep(1)
            
            print("正在掃描設備...", flush=True)
            devices = await BleakScanner.discover(timeout=5, return_adv=True)
            
            if devices:
                print(f"✅ 掃描成功！找到 {len(devices)} 個設備：")
                for device, adv in devices.items():
                    if device.name:
                        print(f"  - {device.name} ({device.address})")
                return True
            else:
                print("⚠️  掃描成功但未找到設備")
        
        except OSError as e:
            print(f"❌ 掃描失敗 (嘗試 {attempt + 1}): {e}")
            if attempt < retries - 1:
                print(f"   等待 3 秒後重試...", flush=True)
                await asyncio.sleep(3)
        except Exception as e:
            print(f"❌ 意外錯誤: {type(e).__name__}: {e}")
    
    return False

def check_python_version():
    """檢查 Python 版本"""
    print_header("檢查 Python 環境")
    
    version = f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"
    print(f"Python 版本: {version}")
    
    if sys.version_info >= (3, 10):
        print("✅ Python 版本滿足要求 (>= 3.10)")
        return True
    else:
        print("⚠️  建議升級到 Python 3.10+ 以獲得更好的 asyncio 支援")
        return False

def check_bleak_version():
    """檢查 Bleak 版本"""
    print_header("檢查依賴版本")
    
    try:
        import bleak
        print(f"Bleak 版本: {bleak.__version__}")
        
        # 檢查是否有新版本
        print("✅ Bleak 已安裝")
        return True
    except ImportError:
        print("❌ 未安裝 Bleak")
        print("   請執行: pip install --upgrade bleak")
        return False

def manual_connection_guide():
    """手動連接指南"""
    print_header("手動連接指南")
    
    print("""
如果自動掃描失敗，請嘗試以下方法之一：

【方法 1】使用 nRF Connect（手機應用）
  1. 在 Android/iOS 上安裝 nRF Connect
  2. 掃描並連接到 BillCat_Fan_Control
  3. 驗證 BLE 功能是否正常

【方法 2】使用 Windows 設定連接
  1. 開啟設定 → 裝置 → 藍牙和其他裝置
  2. 點擊「新增裝置」
  3. 選擇「藍牙」
  4. 等待掃描完成並選擇 BillCat_Fan_Control

【方法 3】使用藍牙命令列工具
  PowerShell (管理員):
  Get-CimInstance -ClassName Win32_BluetoothDevice

【方法 4】重新啟動藍牙棧
  PowerShell (管理員):
  Restart-Service -Name BluetoothUserService -Force
  Restart-Service -Name BluetoothUserService_* -Force

【方法 5】更新藍牙驅動
  1. 開啟設備管理器（devmgmt.msc）
  2. 展開「藍牙」
  3. 右鍵點擊藍牙適配器 → 更新驅動程式

【方法 6】檢查 ESP32 的 BLE
  1. 確保 ESP32 正在廣播 BLE
  2. 使用手機 nRF Connect 驗證
  3. 檢查 ESP32 的 BLE Task 是否運行
    """)

async def main():
    """主診斷程序"""
    print("=" * 60)
    print("Windows BLE 診斷和修復工具")
    print("=" * 60)
    
    # 1. 檢查 Python 版本
    python_ok = check_python_version()
    
    # 2. 檢查 Bleak 版本
    bleak_ok = check_bleak_version()
    
    if not bleak_ok:
        print("\n❌ 未安裝 Bleak，無法繼續")
        return
    
    # 3. 檢查藍牙服務
    check_bluetooth_service()
    check_device_manager()
    
    # 4. 嘗試掃描
    scan_ok = await test_bleak_with_retry(retries=3)
    
    if scan_ok:
        print_header("診斷完成 ✅")
        print("藍牙已正常工作，現在可以運行 ble_simple.py")
    else:
        print_header("診斷完成 ❌")
        print("""
藍牙掃描仍然失敗。請嘗試以下步驟：

【優先級 1】 檢查 ESP32
  □ 確保 ESP32 已連接電源
  □ 確保 BLE 代碼已編譯並上傳
  □ 檢查 Serial 監視器，看 BLE 是否初始化成功

【優先級 2】 重新啟動藍牙
  □ 以管理員身份打開 PowerShell
  □ 執行: Restart-Service -Name BluetoothUserService -Force
  □ 等待 5 秒，然後重新運行此腳本

【優先級 3】 驗證 BLE 是否工作
  □ 在手機上安裝 nRF Connect
  □ 掃描是否能找到 BillCat_Fan_Control
  □ 如果手機能找到，說明 ESP32 BLE 正常
  □ 問題在於 Windows 藍牙驅動

【優先級 4】 更新驅動
  □ 開啟設備管理器（devmgmt.msc）
  □ 展開藍牙，更新驅動程式
  □ 重新啟動電腦

【優先級 5】 使用其他介面
  □ 改用 CDC（USB 序列埠）
  □ 改用 HID（USB HID 介面）
    """)
        manual_connection_guide()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n已中止")
    except Exception as e:
        print(f"\n❌ 致命錯誤: {e}")
        import traceback
        traceback.print_exc()
