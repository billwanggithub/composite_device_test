# ESP32-S3 USB 複合裝置

一個基於 ESP32-S3 的 USB 複合裝置實作，結合了 CDC（序列埠）和自訂 HID（人機介面裝置）功能，採用 FreeRTOS 多工架構。

## ✨ 主要特性

- **雙介面設計**：同時提供 CDC 和 HID 介面
- **統一命令系統**：兩個介面共用相同的命令集
- **雙協定支援**：
  - **0xA1 協定**：結構化命令格式，適合應用程式
  - **純文本協定**：直接文字命令，適合快速測試
- **多通道回應**：HID 命令同時回應到 CDC 和 HID 介面
- **FreeRTOS 多工**：使用獨立 Task 處理 HID 和 CDC 資料
- **執行緒安全**：完整的 Mutex 保護機制
- **64-byte HID 報告**：無 Report ID，真正的 64 位元組傳輸

## 📚 語言說明 / Language Notes

本專案採用雙語文件策略：

- **CLAUDE.md** - 英文（供 AI 助手和國際開發者使用）
- **README.md, PROTOCOL.md, TESTING.md** - 繁體中文（供主要使用者）

This project uses a bilingual documentation strategy:

- **CLAUDE.md** - English (for AI assistants and international developers)
- **README.md, PROTOCOL.md, TESTING.md** - Traditional Chinese (for primary users)

## 📋 系統需求

### 硬體
- **ESP32-S3-DevKitC-1 N16R8** （預設配置）
  - 晶片：ESP32-S3
  - Flash：16 MB Quad SPI Flash
  - PSRAM：8 MB Octal PSRAM
  - USB：支援 USB OTG
- USB 資料線（非充電線）

**⚠️ 其他開發板型號：**
如果您使用的是其他 ESP32-S3-DevKitC-1 型號（例如 N8R2、N8、或標準版），請參閱下方的 [更換開發板型號](#更換開發板型號) 章節。

### 軟體
- [PlatformIO](https://platformio.org/) IDE 或 CLI
- Python 3.7+ （用於測試腳本）
- Python 套件：`pyserial`, `pywinusb`（Windows HID 庫）

## 🚀 快速開始

### 1. 編譯韌體

```bash
# 清除舊建置
pio run -t clean

# 編譯
pio run
```

**預期輸出：**
```
RAM:   [=] 9.5% (31,216 bytes)
Flash: [=] 9.2% (307,045 bytes)
[SUCCESS]
```

### 2. 上傳韌體

**⚠️ 重要：進入 Bootloader 模式**

1. 按住 **BOOT** 按鈕
2. 按下 **RESET** 按鈕
3. 放開 **BOOT** 按鈕

**上傳：**
```bash
pio run -t upload
```

### 3. 重新插拔 USB 線（必須！）

⚠️ **非常重要**：上傳完成後，必須實體重新插拔 USB 線！

1. 拔掉 USB 線
2. 等待 2-3 秒
3. 重新插上 USB 線

這確保 USB 裝置正確列舉並顯示正確的 VID:PID (303A:4002)。

### 4. 驗證裝置

**Windows 裝置管理員應該顯示：**

```
連接埠 (COM 和 LPT)
  └─ USB 序列裝置 (COMX)

人機介面裝置
  └─ TinyUSB HID
```

**檢查 VID:PID：**
```bash
python -m serial.tools.list_ports -v
```

應該看到：
```
COMX
  描述: USB 序列裝置
  VID:PID: 303A:4002  ✓
```

### 5. 安裝測試依賴

```bash
pip install -r requirements.txt
```

或手動安裝：
```bash
pip install pyserial pywinusb
```

### 6. 執行測試

**快速測試所有功能：**
```bash
python test_all.py
```

**測試 CDC 介面：**
```bash
python test_cdc.py
```

**測試 HID 介面（純文本協定）：**
```bash
python test_hid.py test
```

**測試 HID 介面（0xA1 協定）：**
```bash
python test_hid.py test-0xa1
```

**互動模式：**
```bash
# CDC 互動模式
python test_cdc.py interactive

# HID 互動模式（純文本）
python test_hid.py interactive

# HID 互動模式（0xA1 協定）
python test_hid.py interactive-0xa1
```

## 📡 可用命令

| 命令 | 說明 | 範例回應 |
|------|------|---------|
| `*IDN?` | SCPI 標準識別 | `HID_ESP32_S3` |
| `HELP` | 顯示命令列表 | 可用命令清單 |
| `INFO` | 裝置資訊 | 晶片型號、記憶體資訊 |
| `STATUS` | 系統狀態 | 運行時間、記憶體、HID 狀態 |
| `SEND` | 發送測試 HID IN 報告 | 確認訊息 |
| `READ` | 讀取 HID 緩衝區 | Hex dump (64 bytes) |
| `CLEAR` | 清除 HID 緩衝區 | 確認訊息 |

所有命令不區分大小寫，必須以換行符 (`\n` 或 `\r`) 結尾。

## 🔧 架構概述

### USB 介面

**CDC (Communications Device Class)：**
- 虛擬序列埠介面
- 純文本命令和回應
- 用於 console 輸出和除錯

**HID (Human Interface Device)：**
- 64-byte 雙向資料報告（無 Report ID）
- 支援兩種協定格式
- 用於應用程式資料傳輸

### FreeRTOS 任務架構

```
┌─────────────────┐              ┌──────────────────┐
│  CDC Console    │              │  HID Interface   │
│  (純文本)        │              │  (64-byte)       │
└────────┬────────┘              └────────┬─────────┘
         │                                │
         ▼                                ▼
   ┌─────────────┐              ┌─────────────────┐
   │  cdcTask    │              │    hidTask      │
   │  (優先權 1)  │              │   (優先權 2)     │
   └──────┬──────┘              └────────┬────────┘
          │                              │
          │  serialMutex                 │  hidSendMutex
          │  保護                        │  bufferMutex
          │                              │
          ▼                              ▼
   ┌──────────────────────────────────────────────┐
   │          CommandParser                       │
   │          (統一命令處理)                        │
   └──────┬───────────────────────────────┬───────┘
          │                               │
          │ CDCResponse                   │ MultiChannelResponse
          │ (僅 CDC)                      │ (CDC + HID)
          │                               │
          ▼                               ▼
   ┌─────────────┐              ┌─────────────────────┐
   │     CDC     │              │   CDC   │    HID    │
   └─────────────┘              └─────────────────────┘
```

### 回應路由

| 命令來源 | CDC 回應 | HID 回應 |
|---------|---------|---------|
| CDC     | ✓ 是    | ✗ 否    |
| HID     | ✓ 是    | ✓ 是    |

## 🔄 更換開發板型號

此專案預設配置為 **ESP32-S3-DevKitC-1 N16R8** (16MB Flash, 8MB PSRAM)，但支援所有 ESP32-S3-DevKitC-1 系列開發板。

### 支援的開發板型號

| 型號 | Flash | PSRAM | PlatformIO 板型 ID |
|------|-------|-------|-------------------|
| **N16R8** (預設) | 16 MB | 8 MB | `esp32-s3-devkitc-1-n16r8` |
| **N8R2** | 8 MB | 2 MB | `esp32-s3-devkitc-1-n8r2` |
| **N8R8** | 8 MB | 8 MB | `esp32-s3-devkitc-1-n8r8` |
| **N8** (標準版) | 8 MB | 無 PSRAM | `esp32-s3-devkitc-1-n8` |
| **通用型** | 4 MB | 2 MB | `esp32-s3-devkitc-1` |

### 更換步驟

**1. 編輯 `platformio.ini`**

修改 `[env:...]` 段落和 `board` 參數。例如更換為 **N8R2**：

```ini
[env:esp32-s3-devkitc-1-n8r2]
platform = espressif32
board = esp32-s3-devkitc-1-n8r2
framework = arduino
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCONFIG_TINYUSB_HID_BUFSIZE=128
    -DCFG_TUD_HID_EP_BUFSIZE=128
monitor_speed = 115200
```

**2. 清除並重新建置（必須！）**

```bash
pio run -t clean && pio run
```

這確保使用正確的分割表和 PSRAM 設定。

**3. 上傳韌體**

```bash
# 進入 bootloader 模式：按住 BOOT，按 RESET，放開 BOOT
pio run -t upload
```

**4. 重新插拔 USB 線**

完成上傳後，務必重新插拔 USB 線以正確初始化裝置。

**5. 驗證設定**

使用 `INFO` 命令檢查 Flash 和 PSRAM 大小是否正確。

### 型號判別

查看開發板上的標籤：
- `ESP32-S3-DevKitC-1-N16R8` → 16MB Flash, 8MB PSRAM
- `ESP32-S3-DevKitC-1-N8R2` → 8MB Flash, 2MB PSRAM
- `ESP32-S3-DevKitC-1-N8` → 8MB Flash, 無 PSRAM

型號代碼說明：
- **N16/N8/N4** = Flash 容量 (16MB/8MB/4MB)
- **R8/R2** = PSRAM 容量 (8MB/2MB)
- 無 R = 無 PSRAM

### 重要事項

- ✅ **程式碼無需修改**：USB 複合裝置實作與記憶體大小無關
- ✅ **韌體大小**：約 307KB，所有型號都有充足空間
- ✅ **RAM 使用**：約 31KB，遠低於 ESP32-S3 的 512KB 內建 SRAM
- ⚠️ **建置旗標**：所有型號使用相同的 build_flags
- ⚠️ **分割表**：PlatformIO 會自動根據板型選擇正確的分割表

詳細技術說明請參閱 [CLAUDE.md - Changing Board Variants](CLAUDE.md#changing-board-variants)。

## 📚 文件

- **[PROTOCOL.md](PROTOCOL.md)** - HID 協定規格詳細說明
- **[TESTING.md](TESTING.md)** - 測試腳本使用指南
- **[CLAUDE.md](CLAUDE.md)** - AI 輔助開發專案說明

## 🔍 故障排除

### 問題：找不到 COM port

**解決方法：**
1. 檢查 USB 線是否支援資料傳輸（不是只有充電線）
2. 重新插拔 USB 線
3. 檢查裝置管理員是否有未知裝置
4. 重新上傳韌體

### 問題：找不到 HID 裝置

**解決方法：**
1. **重新插拔 USB 線**（最重要！）
2. 確認裝置管理員中有 "TinyUSB HID"
3. 確認韌體正確上傳
4. 檢查 VID:PID 是否為 303A:4002（不是 303A:1001）

### 問題：裝置管理員顯示黃色驚嘆號

**可能原因：**
- HID Report Descriptor 錯誤
- USB Descriptor 不完整

**解決方法：**
1. 重新編譯和上傳韌體：`pio run -t clean && pio run && pio run -t upload`
2. 重新插拔 USB 線

### 問題：命令無回應

**檢查清單：**
- [ ] 韌體已成功上傳
- [ ] USB 線已重新插拔
- [ ] VID:PID 為 303A:4002
- [ ] DTR 已啟用（測試腳本自動啟用）
- [ ] 命令以 `\n` 結尾
- [ ] HID 命令使用正確的協定格式

## 🎯 專案結構

```
composite_device_test/
├── src/
│   ├── main.cpp              # 主程式（USB 初始化、Task 管理）
│   ├── CustomHID.h/cpp       # 64-byte 自訂 HID 類別
│   ├── CommandParser.h/cpp   # 統一命令解析器
│   ├── HIDProtocol.h/cpp     # HID 協定處理
│   └── ...
├── test_hid.py               # HID 測試腳本
├── test_cdc.py               # CDC 測試腳本
├── test_all.py               # 整合測試腳本
├── requirements.txt          # Python 依賴套件清單
├── platformio.ini            # PlatformIO 配置
├── README.md                 # 本文件
├── PROTOCOL.md               # 協定規格
├── TESTING.md                # 測試指南
└── CLAUDE.md                 # AI 開發說明
```

## 🛠️ 開發

### 新增命令

1. 在 `CommandParser.cpp` 的 `processCommand()` 中新增命令處理
2. 使用 `response->print()`, `response->println()`, 或 `response->printf()` 輸出
3. 回應會自動路由到正確的介面

### 修改 HID 協定

1. 編輯 `HIDProtocol.h/cpp` 的協定解析和編碼函數
2. 更新 `PROTOCOL.md` 文件

### 調整 Task 優先權

在 `main.cpp` 的 `setup()` 中修改：
```cpp
xTaskCreatePinnedToCore(hidTask, "HID_Task", 4096, NULL, 2, NULL, 1);  // 優先權 = 2
xTaskCreatePinnedToCore(cdcTask, "CDC_Task", 4096, NULL, 1, NULL, 1);  // 優先權 = 1
```

## 📄 授權

此專案為開源專案，供學習和開發使用。

## 🤝 貢獻

歡迎提交 Issue 和 Pull Request！

---

**開發板**：ESP32-S3-DevKitC-1 N16R8
**晶片**：ESP32-S3

## 🔵 BLE GATT "Serial" (ESP32-S3)

ESP32-S3 does not support Classic Bluetooth SPP. This project exposes a BLE GATT-based
console instead: a writeable RX characteristic and a notify-only TX characteristic.

- Service UUID: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- RX (write) UUID: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- TX (notify) UUID: `beb5483e-36e1-4688-b7f5-ea07361b26a9`
- Default BLE device name: `ESP32_S3_Console`

A small Python test client that uses `bleak` is provided at `scripts/ble_client.py`.
It will:

- scan (or use a provided address) and connect to the device
- subscribe to TX notifications and print incoming data
- read stdin lines and write them to the RX characteristic (commands must end with `\n`)

Quick example (install bleak first):

```powershell
pip install bleak
python scripts/ble_client.py --name ESP32_S3_Console
```

See `scripts/ble_client.py` for details and options (you can also pass `--address`).

### Mobile testing with nRF Connect (Android / iOS)

You can manually test the BLE RX/TX console using the free nRF Connect mobile app.
Follow these steps:

1. Install nRF Connect from Google Play or the App Store.
2. Open the app and start a scan. Look for the device named `ESP32_S3_Console` (or your device's configured name).
3. Tap the device to connect.
4. After connection expand the service with UUID `4fafc201-1fb5-459e-8fcc-c5c9c331914b`.
5. Locate the TX characteristic (notify) with UUID `beb5483e-36e1-4688-b7f5-ea07361b26a9` and enable Notifications (tap the bell / subscribe icon).
  - You should now see notifications coming from the device when it sends data (for example, command responses).
6. Locate the RX characteristic (write) with UUID `beb5483e-36e1-4688-b7f5-ea07361b26a8`.
  - Use the "Write" UI to send text; in the nRF Connect write box you can switch to "UTF-8" and enter text.
  - IMPORTANT: Commands must end with a newline (press Enter or include `\n`), e.g. `HELP\n`.
  - For this firmware you can write without response (Write Without Response) — both modes typically work, but the firmware's GATT write uses response=False in the test client.
7. Observe responses in the TX notifications area. The device also prints to USB CDC if a host is connected.

Tips:
- On iOS the address is hidden; scan by device name. On Android you can prefer address if you have it.
- If you don't see notifications, reconnect and ensure the TX characteristic is subscribed.
- Use nRF Connect to send `*IDN?\n` or `HELP\n` to verify the command parser and responses.

**Flash**：16 MB Quad SPI Flash
**PSRAM**：8 MB Octal PSRAM
**開發工具**：PlatformIO
**框架**：Arduino (ESP32)
**核心組件**：TinyUSB、FreeRTOS
