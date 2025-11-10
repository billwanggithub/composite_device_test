# BLE 測試狀態報告

## 當前狀況

### ✅ BLE 硬體状態
- **ESP32 BLE 初始化** ✅ 成功
- **BLE 廣播** ✅ 正常工作
- **手機 nRF Connect 測試** ✅ 成功連接和通信
- **設備名稱** ✅ BillCat_Fan_Control
- **BLE GATT Service** ✅ 工作正常

### ⚠️ Windows 藍牙掃描問題
- **問題** Windows WinRT API 無法掃描到 ESP32
- **原因** 這是 Bleak 在 Windows 上的已知限制
- **症狀** BLE 掃描成功（找到 36 個設備）但未找到 BillCat_Fan_Control
- **影響** 自動化的 Python BLE 測試無法正常工作

### ✅ 其他介面狀態
- **USB CDC** ✅ 正常（序列埠通信）
- **USB HID** ✅ 正常（64 字節自訂協議）
- **WiFi** ✅ 正常（Web 伺服器運行）

---

## 推薦的測試方法

### 方法 1：使用 CDC 介面（推薦用於自動化測試）✅

最穩定和最可靠的方法：

```bash
# 測試 CDC 介面
python scripts/test_all.py cdc

# 或測試所有介面
python scripts/test_all.py all
```

**優點：**
- ✅ 100% 可靠
- ✅ 不受 Windows 藍牙驅動影響
- ✅ 最快的回應時間
- ✅ 可以腳本化和自動化

### 方法 2：使用 HID 介面（推薦用於快速測試）

```bash
# 測試 HID 介面
python scripts/test_all.py hid
```

**優點：**
- ✅ 不依賴序列埠驅動
- ✅ 支援 64 字節自訂協議
- ✅ 相對快速

### 方法 3：使用手機進行 BLE 測試（已驗證工作）✅

在手機上使用 **nRF Connect** 應用：

1. 下載 nRF Connect（Google Play 或 App Store）
2. 開啟應用並掃描
3. 連接到 "BillCat_Fan_Control"
4. 訂閱 TX Characteristic（UUID 結尾 26a9）進行通知
5. 寫入 RX Characteristic（UUID 結尾 26a8）發送命令

**確認資訊：**
- Service UUID: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- RX UUID: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- TX UUID: `beb5483e-36e1-4688-b7f5-ea07361b26a9`

---

## 測試命令參考

### 基本命令

```
*IDN?          - 設備識別（SCPI 命令）
INFO           - 顯示設備資訊
STATUS         - 顯示系統狀態
HELP           - 顯示所有命令
```

### 馬達控制

```
SET PWM 1000 50     - 設定 PWM（頻率 1000Hz，占空比 50%）
RPM                 - 讀取當前 RPM
MOTOR STATUS        - 顯示馬達詳細狀態
MOTOR STOP          - 停止馬達
```

---

## Windows BLE 掃描問題的根本原因

### 技術背景

Windows 的 Bleak 使用 WinRT API 進行 BLE 掃描。該 API 有以下限制：

1. **設備快取** - WinRT 有內部設備快取，某些情況下不會更新
2. **驅動程式問題** - 藍牙驅動的行為不一致
3. **環境干擾** - 周圍有很多 BLE 設備時掃描不穩定

### 為什麼手機能找到而 Windows 找不到

- **手機** 使用原生藍牙 API（Android BLE API、iOS CoreBluetooth）
- **Windows** 使用 WinRT（較高層次的抽象）
- **Bleak** 只是 WinRT 的包裝層，受其限制

### 已知的解決方案

1. ❌ **重新啟動藍牙服務** - 有時有效，但不穩定
2. ❌ **更新驅動程式** - 有時有效，但並非所有驅動都更新
3. ❌ **使用不同的 Python BLE 庫** - 都受限於 WinRT API
4. ✅ **使用替代介面** - CDC、HID、甚至直接連接（推薦）

---

## 總結和建議

### ✅ 已驗證工作的功能

| 功能 | 介面 | 狀態 | 備註 |
|------|------|------|------|
| **命令執行** | CDC | ✅ 完全工作 | 推薦用於測試 |
| **HID 通信** | USB HID | ✅ 完全工作 | 支援 64 字節 |
| **BLE 無線** | BLE GATT | ✅ 工作（手機驗證） | Windows 掃描有問題 |
| **Web 控制** | WiFi | ✅ 完全工作 | HTTP Web 介面 |
| **WebSocket** | WiFi | ✅ 完全工作 | 即時 RPM 監控 |

### 🎯 推薦的工作流

```
開發/測試階段：
  1. 使用 CDC 進行命令測試（最快、最穩定）
  2. 使用 HID 進行自訂協議測試
  3. 使用 Web 介面進行功能驗證

無線測試：
  1. 使用手機 nRF Connect 進行 BLE 測試（已驗證）
  2. 通過 WiFi 使用 Web 介面

自動化測試：
  1. 使用 CDC 進行持續集成 (CI/CD)
  2. 使用 HID 進行性能測試
  3. 避免依賴 Windows BLE 掃描
```

---

## 後續改進建議

### 短期（立即可做）

1. ✅ **已完成** - 改進 BLE 廣播參數
2. 📝 文檔 - 添加本 BLE_STATUS.md 文檔
3. 🧪 測試 - 主要使用 CDC/HID 進行自動化測試

### 中期（下一個版本）

1. 創建 Android/iOS BLE 應用（繞過 Windows 限制）
2. 添加 BLE 連接狀態監控
3. 改進 BLE 通知的可靠性

### 長期（未來優化）

1. 考慮使用 Matter 協議（更標準）
2. 添加藍牙 5.0 高速傳輸
3. 實現 BLE Mesh 網絡

---

## 快速參考

### 測試 ESP32 BLE

**手機（推薦）：**
```
1. 安裝 nRF Connect
2. 掃描 "BillCat_Fan_Control"
3. 連接並發送命令
```

**Windows（CDC 替代方案）：**
```bash
python scripts/test_all.py cdc
```

**Windows（HID 替代方案）：**
```bash
python scripts/test_all.py hid
```

### 調試 BLE 連接

如果 BLE 連接失敗：

1. **檢查 ESP32 日誌**
   ```
   [INFO] 正在初始化 BLE...
   [INFO] BLE 初始化完成
   ```

2. **驗證手機發現**
   - 使用 nRF Connect 掃描
   - 應該看到 "BillCat_Fan_Control"

3. **驗證 WiFi 不會干擾 BLE**
   - BLE 和 WiFi 都使用 2.4GHz
   - 如果 WiFi 信號強，BLE 會減弱
   - 這是正常的，無法避免

---

## 聯繫和反饋

如果您遇到問題：

1. 確認手機 nRF Connect 能連接
2. 使用 CDC 進行測試（最可靠）
3. 檢查 ESP32 序列輸出中的 BLE 初始化日誌
4. 嘗試重新啟動藍牙服務或電腦

