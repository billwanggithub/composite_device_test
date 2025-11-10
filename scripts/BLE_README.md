# BLE 測試指南

## 快速開始

### 安裝依賴
```bash
pip install bleak
```

## 使用簡化版 BLE 客戶端（推薦）

### 互動模式（預設）
```bash
python ble_simple.py
```

**功能：**
- ✅ 自動掃描並列出所有 BLE 設備
- ✅ 自動連接到 `BillCat_Fan_Control`
- ✅ 互動式命令輸入
- ✅ 即時接收並顯示回應
- ✅ 穩定的 event loop 管理

**使用示例：**
```
掃描 BLE 設備 'BillCat_Fan_Control'...

掃描到 3 個設備:
  - BillCat_Fan_Control (AA:BB:CC:DD:EE:FF)
  - Other Device (11:22:33:44:55:66)

連接 BillCat_Fan_Control...
✅ 已連接

============================================================
BLE 互動模式
============================================================
輸入命令（例如: HELP, INFO, STATUS, RPM）
輸入 'exit' 退出

> INFO
📤 發送: INFO
等待回應...
[通知] 裝置資訊:
[通知] 晶片型號: ESP32-S3
```

### 自動測試模式
```bash
python ble_simple.py test
```

**功能：**
- 自動執行一系列測試命令（*IDN?, INFO, STATUS）
- 顯示所有回應
- 測試完成後自動斷開連接

## 使用完整版 BLE 客戶端

原始的 `ble_client.py` 也支援 BLE 測試：

```bash
python ble_client.py
```

這是功能最完整的版本，但在 Windows 上可能需要額外配置。

## 故障排除

### 問題：掃描失敗 "[WinError -2147020577] 裝置尚未就緒"

**原因：** Windows 藍牙驅動初始化問題

**解決方案：**
1. 確保 ESP32 已開啟並正在廣播 BLE
2. 檢查 Windows 藍牙設定是否開啟
3. 嘗試重新啟動藍牙服務：
   ```
   # Windows PowerShell (管理員)
   Restart-Service -Name BluetoothUserService -Force
   Restart-Service -Name BluetoothUserService_* -Force
   ```
4. 重新啟動 Python 腳本

### 問題：連接成功但無法接收回應

**檢查清單：**
1. ✅ ESP32 BLE 已初始化
2. ✅ TX 特性已訂閱通知
3. ✅ 命令格式正確（以 `\n` 結尾）
4. ✅ 檢查 ESP32 的 BLE Task 是否執行

**測試 BLE 是否工作：**
```bash
# 使用 nRF Connect App（手機）
1. 掃描並連接 BillCat_Fan_Control
2. 訂閱 TX 特性（UUID 結尾 26a9）
3. 寫入命令到 RX 特性（UUID 結尾 26a8）
4. 應該能在 TX 特性上收到通知
```

### 問題：Windows 上 event loop 錯誤

**症狀：**
```
RuntimeError: This event loop is already running
RuntimeError: There is no current event loop in thread
```

**解決方案：**
- 使用 `ble_simple.py`（已修復此問題）
- 或升級 Python 到 3.10+（更好的 asyncio 支援）

## 命令參考

### 基本命令
```
HELP          - 顯示所有可用命令
INFO          - 顯示設備資訊
STATUS        - 顯示系統狀態
*IDN?         - SCPI 識別字串

RPM           - 讀取當前 RPM
SET PWM 1000 50  - 設定 PWM（頻率, 佔空比）

MOTOR STATUS  - 顯示馬達詳細狀態
MOTOR STOP    - 停止馬達
```

### WiFi 命令
```
IP            - 顯示 IP 位址
WIFI <ssid> <password>  - 連接到 WiFi
```

## 比較：CDC vs HID vs BLE

| 功能 | CDC | HID | BLE |
|------|-----|-----|-----|
| 有線連接 | ✅ | ✅ | ❌ |
| 無線連接 | ❌ | ❌ | ✅ |
| 易用性 | ✅ | ⭐⭐ | ⭐⭐⭐ |
| 回應速度 | 最快 | 快 | 普通 |
| 範圍 | USB | USB | 10-100m |

**選擇建議：**
- **開發/調試** → 使用 CDC（最穩定）
- **快速測試** → 使用 HID
- **無線測試** → 使用 BLE

## 手機應用測試

### 使用 nRF Connect（Android/iOS）

1. **下載應用：** Google Play / App Store 搜尋 "nRF Connect"
2. **掃描：** 應用會自動掃描附近的 BLE 設備
3. **連接：** 選擇 "BillCat_Fan_Control"
4. **服務發現：** 應用會列出所有 Service/Characteristic
5. **訂閱通知：** 點擊 TX Characteristic（結尾 26a9），選擇訂閱
6. **發送命令：** 點擊 RX Characteristic（結尾 26a8），輸入命令並送出

**預期特性：**
- Service UUID: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- RX Characteristic: `beb5483e-36e1-4688-b7f5-ea07361b26a8` (Write)
- TX Characteristic: `beb5483e-36e1-4688-b7f5-ea07361b26a9` (Notify)

## 性能提示

- 🚀 **互動模式延遲：** 100-500ms（取決於 BLE 連接質量）
- 📊 **通知接收：** 通常在 50-200ms 內
- ⏱️ **掃描時間：** 首次掃描 8 秒，後續重新掃描也是 8 秒

**優化建議：**
- 確保 ESP32 靠近接收設備（< 5 米）
- 關閉其他藍牙設備減少干擾
- 使用 2.4GHz WiFi 時可能會有干擾，調整 WiFi 頻道

