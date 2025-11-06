# test_all.py 使用指南

## 功能概述

改進版 `test_all.py` 支援測試 ESP32-S3 的三個通訊介面：
- **CDC (Serial)** - USB 虛擬串列埠
- **HID** - USB 自定義 HID 裝置
- **BLE** - 藍牙低功耗 GATT 服務

## 安裝依賴

```bash
# 基本依賴（CDC + HID）
pip install pyserial pywinusb

# BLE 測試依賴（可選）
pip install bleak
```

## 使用方法

### 1. 測試所有可用介面（預設）
```bash
python scripts/test_all.py
# 或
python scripts/test_all.py all
```
自動掃描並測試所有找到的介面，比較多通道回應。

### 2. 僅測試特定介面

```bash
# 僅測試 CDC
python scripts/test_all.py cdc

# 僅測試 HID
python scripts/test_all.py hid

# 僅測試 BLE
python scripts/test_all.py ble
```

### 3. 查看幫助
```bash
python scripts/test_all.py help
```

## 主要改進

### 穩定性改善

1. **智慧回應收集**
   - 使用「閒置計數器」而非固定超時
   - 連續 10 次（0.5 秒）沒有新資料才結束接收
   - 確保接收完整的多行回應

2. **增加等待時間**
   - 命令發送後等待 0.5 秒（原本 0.3 秒）
   - 命令間隔 0.5 秒（原本 0.3 秒）
   - 避免命令和回應重疊

3. **改善緩衝區管理**
   - 發送前清空緩衝區
   - 清空後等待 0.1 秒穩定

### BLE 支援

1. **自動掃描連接**
   - 掃描名稱為 "ESP32_S3_Console" 的裝置
   - 自動連接並設定通知

2. **異步操作**
   - 使用 `bleak` 庫的異步 API
   - 同步包裝函數方便使用

3. **三介面比較**
   - 支援比較 CDC、HID、BLE 的回應
   - 自動檢測回應一致性

## 測試流程

### 完整測試流程
1. 掃描 CDC 介面（COM port）
2. 掃描 HID 介面（VID/PID: 0x303A/0x1001）
3. 掃描 BLE 介面（裝置名稱: ESP32_S3_Console）
4. 對每個命令執行測試：
   - `*IDN?` - 裝置識別
   - `INFO` - 詳細資訊
   - `STATUS` - 系統狀態
   - `HELP` - 命令列表
5. 比較各介面的回應

### 測試命令

所有測試使用相同的命令集：
- `*IDN?` - 簡短識別回應（單行）
- `INFO` - 長回應（多行）
- `STATUS` - 長回應（多行）
- `HELP` - 長回應（多行）

## 輸出格式

```
============================================================
命令: INFO
============================================================

📡 CDC 回應:
  === ESP32-S3 裝置資訊 ===
  硬體規格:
  ...

📡 HID 回應:
  === ESP32-S3 裝置資訊 ===
  硬體規格:
  ...

📡 BLE 回應:
  === ESP32-S3 裝置資訊 ===
  硬體規格:
  ...

✅ 所有介面回應一致
```

## 常見問題

### 1. CDC 介面找不到

**原因**：
- COM port 被其他程式佔用（如 PlatformIO monitor）
- USB 線材品質問題
- 裝置未正確枚舉

**解決方法**：
```bash
# 關閉 PlatformIO monitor
Ctrl+C

# 重新插拔 USB 線
```

### 2. BLE 連接失敗

**原因**：
- 電腦沒有藍牙適配器
- 藍牙適配器被禁用
- 裝置距離太遠

**解決方法**：
```bash
# 檢查藍牙狀態
# Windows: 設定 → 裝置 → 藍牙

# 測試時跳過 BLE
python scripts/test_all.py cdc
python scripts/test_all.py hid
```

### 3. HID 裝置斷開連接錯誤

**錯誤訊息**：
```
pywinusb.hid.helpers.HIDError: Error device disconnected before write
```

**原因**：
- 裝置在測試過程中被重置
- USB 連接不穩定
- 前一個測試沒有正確關閉 HID

**解決方法**：
```bash
# 重新插拔 USB 線
# 重新執行測試
```

### 4. 回應不完整或無回應

**原因**：
- 等待時間不足
- 裝置處理速度慢
- 通訊緩衝區問題

**解決方法**：
- 增加 `timeout_sec` 參數（在程式碼中）
- 增加命令間隔時間
- 檢查裝置日誌

## 進階使用

### 自定義測試命令

編輯 `test_xxx_only()` 函數中的命令列表：

```python
commands = ["*IDN?", "INFO", "STATUS", "HELP", "YOUR_COMMAND"]
```

### 調整超時時間

在函數呼叫時指定：

```python
responses = test_cdc_command(ser, cmd, timeout_sec=5.0)  # 5 秒超時
```

### 修改 BLE 裝置名稱

修改 `BLE_DEVICE_NAME` 常數：

```python
BLE_DEVICE_NAME = "Your_Device_Name"
```

## 測試結果解讀

### ✅ 成功標記
- `✅ 找到！` - 裝置掃描成功
- `✅ 所有介面回應一致` - 多通道功能正常
- `✅ N 個介面正常運作` - 測試完成

### ⚠️ 警告標記
- `⚠️ 無回應` - 命令沒有收到回應
- `⚠️ 回應不同` - 不同介面的回應不一致
- `⚠️ 警告：未找到 X 介面` - 部分介面不可用

### ❌ 錯誤標記
- `❌ 未找到 X 介面` - 介面掃描失敗
- `❌ 無法連接` - 連接失敗
- `❌ BLE 命令失敗` - BLE 通訊錯誤

## 性能指標

### 單命令測試時間
- CDC: ~0.6-1.0 秒
- HID: ~0.6-1.0 秒
- BLE: ~0.8-1.5 秒

### 完整測試時間
- 單介面測試: ~5-8 秒
- 雙介面測試: ~10-15 秒
- 三介面測試: ~15-25 秒

## 開發建議

### 增加新的測試命令

1. 在 ESP32-S3 韌體中實作新命令
2. 在 `test_xxx_only()` 函數中加入命令
3. 執行測試驗證

### 偵錯技巧

1. **啟用詳細輸出**
   - 查看 ESP32 串列監控輸出
   - 檢查命令接收和處理

2. **單步測試**
   - 先測試單一介面
   - 確認每個命令都正常
   - 最後測試多通道

3. **檢查時序**
   - 使用邏輯分析儀
   - 查看 Bus Hound (HID)
   - 使用 Wireshark (BLE)

## 版本歷史

### v2.0 (2025-11-06)
- ✨ 加入 BLE 測試支援
- 🔧 改善回應收集穩定性
- 🐛 修正時序問題
- 📝 更新文件

### v1.0 (初始版本)
- ✅ CDC 和 HID 測試
- ✅ 多通道回應比較
