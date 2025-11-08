# ESP32-S3 USB 複合裝置

一個基於 ESP32-S3 的 USB 複合裝置實作，結合了 CDC（序列埠）和自訂 HID（人機介面裝置）功能，採用 FreeRTOS 多工架構。

## ✨ 主要特性

### 通訊介面
- **多介面設計**：同時提供 USB CDC、USB HID 和 BLE GATT 介面
- **統一命令系統**：所有介面共用相同的命令集
- **雙協定支援**：
  - **0xA1 協定**：結構化命令格式，適合應用程式
  - **純文本協定**：直接文字命令，適合快速測試
- **統一 CDC 輸出**：除 SCPI 命令外，所有命令統一回應到 CDC（便於監控和除錯）
- **FreeRTOS 多工**：使用獨立 Task 處理 HID 和 CDC 資料
- **執行緒安全**：完整的 Mutex 保護機制
- **64-byte HID 報告**：無 Report ID，真正的 64 位元組傳輸
- **BLE 無線控制**：透過 BLE GATT 特性發送命令

### 馬達控制功能
- **高精度 PWM 輸出**：使用 MCPWM 週邊，頻率範圍 10 Hz - 500 kHz
- **即時 RPM 測量**：硬體 MCPWM Capture 轉速計輸入
- **WiFi Web 介面**：支援 AP 模式和 Station 模式
  - **AP 模式**：建立 WiFi 熱點 (192.168.4.1)，具備 Captive Portal
  - **Station 模式**：連接現有 WiFi 網路
- **Web 控制面板**：完整的 HTML/CSS/JS 控制介面
- **REST API**：RESTful HTTP 端點用於遠端控制
- **設定持久化**：儲存至 NVS（非揮發性儲存）
- **狀態 LED 指示**：WS2812 RGB LED 顯示系統狀態

### 週邊控制功能
- **UART1 多工器**：TX1/RX1 支援 UART、PWM/RPM 或關閉模式（預設 PWM/RPM）
- **UART2 通訊**：TX2/RX2 標準序列埠通訊
- **蜂鳴器 PWM**：可調頻率 (10-20000 Hz) 和佔空比
- **LED PWM 控制**：可調頻率 (100-20000 Hz) 和亮度
- **繼電器控制**：數位開關輸出
- **GPIO 輸出**：通用數位 I/O
- **使用者按鍵**：3 個按鍵輸入，支援防抖和長按檢測
- **週邊設定持久化**：所有週邊參數可儲存至 NVS
- **命令延遲**：DELAY 命令支援腳本化控制序列

## 📚 語言說明 / Language Notes

本專案採用雙語文件策略：

- **CLAUDE.md** - 英文（供 AI 助手和國際開發者使用）
- **README.md, PROTOCOL.md, TESTING.md** - 繁體中文（供主要使用者）

This project uses a bilingual documentation strategy:

- **CLAUDE.md** - English (for AI assistants and international developers)
- **README.md, PROTOCOL.md, TESTING.md** - Traditional Chinese (for primary users)

📖 **完整文件索引請參閱 [DOCS_INDEX.md](DOCS_INDEX.md)** - 幫助您快速找到所需文件

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
python scripts/test_all.py
```

**測試 CDC 介面：**
```bash
python scripts/test_cdc.py
```

**測試 HID 介面（純文本協定）：**
```bash
python scripts/test_hid.py test
```

**測試 HID 介面（0xA1 協定）：**
```bash
python scripts/test_hid.py test-0xa1
```

**互動模式：**
```bash
# CDC 互動模式
python scripts/test_cdc.py interactive

# HID 互動模式（純文本）
python scripts/test_hid.py interactive

# HID 互動模式（0xA1 協定）
python scripts/test_hid.py interactive-0xa1
```

## 📡 可用命令

### 基本命令

| 命令 | 說明 | 範例回應 |
|------|------|---------|
| `*IDN?` | SCPI 標準識別 | `HID_ESP32_S3` |
| `HELP` | 顯示命令列表 | 可用命令清單 |
| `INFO` | 裝置資訊 | 晶片型號、記憶體資訊 |
| `STATUS` | 系統狀態 | 運行時間、記憶體、HID 狀態 |
| `SEND` | 發送測試 HID IN 報告 | 確認訊息 |
| `READ` | 讀取 HID 緩衝區 | Hex dump (64 bytes) |
| `CLEAR` | 清除 HID 緩衝區 | 確認訊息 |
| `DELAY <ms>` | 延遲指定毫秒數 (1-60000ms) | `DELAY 1000` |

### 馬達控制命令

| 命令 | 說明 | 範例 |
|------|------|------|
| `SET PWM_FREQ <Hz>` | 設定 PWM 頻率 (10-500000 Hz) | `SET PWM_FREQ 15000` |
| `SET PWM_DUTY <%>` | 設定 PWM 佔空比 (0-100%) | `SET PWM_DUTY 75.5` |
| `SET POLE_PAIRS <num>` | 設定馬達極對數 (1-12) | `SET POLE_PAIRS 2` |
| `SET MAX_FREQ <Hz>` | 設定最大頻率限制 | `SET MAX_FREQ 100000` |
| `SET LED_BRIGHTNESS <val>` | 設定 LED 亮度 (0-255) | `SET LED_BRIGHTNESS 50` |
| `RPM` | 取得目前 RPM 讀數 | `RPM` |
| `MOTOR STATUS` | 顯示詳細馬達狀態 | `MOTOR STATUS` |
| `MOTOR STOP` | 緊急停止（設定佔空比為 0%） | `MOTOR STOP` |

### WiFi 網路命令

| 命令 | 說明 | 範例 |
|------|------|------|
| `WIFI <ssid> <password>` | 立即連接 WiFi | `WIFI MyNetwork MyPassword` |
| `WIFI CONNECT` | 使用儲存的憑證連接 | `WIFI CONNECT` |
| `START_WEB` | 手動啟動 Web 伺服器 | `START_WEB` |
| `AP ON` | 啟用 AP 模式 | `AP ON` |
| `AP OFF` | 停用 AP 模式 | `AP OFF` |
| `AP STATUS` | 顯示 AP 狀態 | `AP STATUS` |
| `IP` | 顯示 IP 位址和網路資訊 | `IP` |

### 週邊控制命令

#### UART1 多工器 (TX1/RX1)

UART1 支援三種模式：UART 通訊、PWM/RPM 輸出，或關閉。

**重要：** UART1 在每次上電時都會自動設定為 **PWM/RPM 模式**（預設），此設定不會持久化。您可以透過命令隨時切換模式。

| 命令 | 說明 | 範例 |
|------|------|------|
| `UART1 MODE <UART\|PWM\|OFF>` | 切換 UART1 模式 | `UART1 MODE PWM` |
| `UART1 CONFIG <baud>` | 設定 UART 模式鮑率 (2400-1500000) | `UART1 CONFIG 115200` |
| `UART1 PWM <freq> <duty> [ON\|OFF]` | 設定 PWM 參數 (1-500000 Hz, 0-100%) | `UART1 PWM 1000 50 ON` |
| `UART1 STATUS` | 顯示 UART1 目前狀態 | `UART1 STATUS` |
| `UART1 WRITE <text>` | UART 模式發送文字資料 | `UART1 WRITE Hello` |

**模式說明：**
- **UART 模式**：TX1/RX1 作為標準序列埠使用 (可設定鮑率)
- **PWM/RPM 模式**：TX1 輸出 PWM 訊號，RX1 測量 RPM (預設模式)
- **OFF 模式**：關閉 UART1，節省資源

#### UART2 (TX2/RX2)

| 命令 | 說明 | 範例 |
|------|------|------|
| `UART2 CONFIG <baud>` | 設定 UART2 鮑率 (2400-1500000) | `UART2 CONFIG 9600` |
| `UART2 STATUS` | 顯示 UART2 狀態 | `UART2 STATUS` |
| `UART2 WRITE <text>` | 發送文字資料 | `UART2 WRITE Test` |

#### 蜂鳴器 (Buzzer)

| 命令 | 說明 | 範例 |
|------|------|------|
| `BUZZER <freq> <duty> [ON\|OFF]` | 設定蜂鳴器參數 (10-20000 Hz, 0-100%) | `BUZZER 2000 50 ON` |
| `BUZZER STATUS` | 顯示蜂鳴器狀態 | `BUZZER STATUS` |

#### LED PWM

| 命令 | 說明 | 範例 |
|------|------|------|
| `LED_PWM <freq> <brightness> [ON\|OFF]` | 設定 LED 參數 (100-20000 Hz, 0-100%) | `LED_PWM 1000 25 ON` |
| `LED_PWM STATUS` | 顯示 LED 狀態 | `LED_PWM STATUS` |

#### 繼電器 (Relay)

| 命令 | 說明 | 範例 |
|------|------|------|
| `RELAY <ON\|OFF>` | 控制繼電器開關 | `RELAY ON` |
| `RELAY STATUS` | 顯示繼電器狀態 | `RELAY STATUS` |

#### GPIO 通用接腳

| 命令 | 說明 | 範例 |
|------|------|------|
| `GPIO <HIGH\|LOW>` | 設定 GPIO 輸出電平 | `GPIO HIGH` |
| `GPIO STATUS` | 顯示 GPIO 狀態 | `GPIO STATUS` |

#### 使用者按鍵

| 命令 | 說明 | 範例 |
|------|------|------|
| `KEYS MODE <DUTY\|FREQ>` | 設定按鍵調整模式（佔空比或頻率） | `KEYS MODE DUTY` |
| `KEYS STEP <duty_step> <freq_step>` | 設定調整步長 (0.1-10%, 10-10000Hz) | `KEYS STEP 1.0 100` |
| `KEYS STATUS` | 顯示按鍵設定和狀態 | `KEYS STATUS` |

#### 週邊設定持久化

| 命令 | 說明 | 範例 |
|------|------|------|
| `PERIPHERAL SAVE` | 儲存所有週邊設定到 NVS | `PERIPHERAL SAVE` |
| `PERIPHERAL LOAD` | 從 NVS 載入週邊設定 | `PERIPHERAL LOAD` |
| `PERIPHERAL RESET` | 重設週邊為出廠預設值 | `PERIPHERAL RESET` |

**注意：** UART1 模式設定會儲存到 NVS，但不會在開機時自動套用。系統每次上電都會強制設定為 PWM/RPM 模式。

### 設定儲存命令

| 命令 | 說明 | 範例 |
|------|------|------|
| `SAVE` | 儲存所有設定到 NVS | `SAVE` |
| `LOAD` | 從 NVS 載入設定 | `LOAD` |
| `RESET` | 重設為出廠預設值 | `RESET` |

所有命令不區分大小寫，必須以換行符 (`\n` 或 `\r`) 結尾。

### 快速開始範例

```bash
# 透過 CDC 序列埠控制馬達
> SET PWM_FREQ 10000
✅ PWM frequency set to: 10000 Hz

> SET PWM_DUTY 50
✅ PWM duty cycle set to: 50.0%

> RPM
RPM Reading:
  Current RPM: 3000.0
  Input Frequency: 100.0 Hz
  Pole Pairs: 2

# 連接 WiFi 並啟動 Web 伺服器
> WIFI MyNetwork MyPassword
✅ WiFi connected
IP: 192.168.1.100

> START_WEB
✅ Web server started
Access: http://192.168.1.100

# 儲存設定
> SAVE
✅ Settings saved to NVS

# 週邊控制範例
# UART1 切換到 UART 模式並發送資料
> UART1 MODE UART
✅ UART1 mode set to: UART

> UART1 CONFIG 9600
✅ UART1 UART baud rate set to: 9600

> UART1 WRITE Hello World
✅ UART1 sent 11 bytes

# 切回 PWM/RPM 模式
> UART1 MODE PWM
✅ UART1 mode set to: PWM/RPM

> UART1 PWM 5000 75 ON
✅ UART1 PWM: 5000Hz, 75.0%, Enabled

# 控制蜂鳴器
> BUZZER 2000 50 ON
✅ Buzzer: 2000Hz, 50.0%, Enabled

> DELAY 1000
Delaying 1000 ms...
Delay completed

> BUZZER 0 0 OFF
✅ Buzzer disabled

# 控制繼電器和 GPIO
> RELAY ON
✅ Relay: ON

> GPIO HIGH
✅ GPIO: HIGH

# 儲存週邊設定
> PERIPHERAL SAVE
✅ Peripheral settings saved to NVS
```

### Web 介面存取

啟用 WiFi 後，可透過以下方式存取 Web 控制面板：

**AP 模式（預設啟用）：**
- SSID：`ESP32_Motor_Control`
- 密碼：`12345678`
- IP 位址：`http://192.168.4.1`
- Captive Portal：Android 裝置會自動開啟控制頁面

**Station 模式：**
- 使用 `IP` 命令查看取得的 IP 位址
- 在瀏覽器開啟：`http://[IP位址]`

**Web API 端點：**
- `GET /api/status` - 取得系統狀態
- `GET /api/rpm` - 取得 RPM 讀數
- `POST /api/pwm` - 設定 PWM 參數
- `POST /api/save` - 儲存設定
- 更多端點請參閱 [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)

## 🔧 架構概述

### 通訊介面

**USB CDC (Communications Device Class)：**
- 虛擬序列埠介面
- 純文本命令和回應
- 用於 console 輸出和除錯

**USB HID (Human Interface Device)：**
- 64-byte 雙向資料報告（無 Report ID）
- 支援兩種協定格式
- 用於應用程式資料傳輸

**BLE GATT (Bluetooth Low Energy)：**
- 無線命令介面
- RX 特性（寫入命令）
- TX 特性（接收回應）
- 適合行動裝置和無線控制

**重要提示**：ESP32-S3 **不支援** Classic Bluetooth (BR/EDR)，因此無法使用 Bluetooth Serial Port Profile (SPP)。

### FreeRTOS 任務架構

```
┌─────────────────┐   ┌──────────────────┐   ┌──────────────────┐
│  CDC Console    │   │  HID Interface   │   │  BLE GATT        │
│  (純文本)        │   │  (64-byte)       │   │  (無線)          │
└────────┬────────┘   └────────┬─────────┘   └────────┬─────────┘
         │                     │                       │
         ▼                     ▼                       ▼
   ┌─────────────┐   ┌─────────────────┐   ┌─────────────────┐
   │  cdcTask    │   │    hidTask      │   │   bleTask       │
   │  (優先權 1)  │   │   (優先權 2)     │   │  (優先權 1)      │
   └──────┬──────┘   └────────┬────────┘   └────────┬────────┘
          │                   │                       │
          │                   │  Mutex 保護           │
          │                   │                       │
          ▼                   ▼                       ▼
   ┌──────────────────────────────────────────────────────────┐
   │               CommandParser (統一命令處理)                 │
   └──────┬───────────────────────────────┬──────────┬────────┘
          │                               │          │
          │ CDCResponse                   │          │ MultiChannelResponse
          │ (僅 CDC)                      │          │ (CDC + HID/BLE)
          │                               │          │
          ▼                               ▼          ▼
   ┌─────────────┐         ┌──────────────────────────────────┐
   │     CDC     │         │  CDC  │  HID  │       BLE        │
   └─────────────┘         └──────────────────────────────────┘
```

### 回應路由

系統採用兩種不同的回應路由策略：

#### 一般命令（HELP, INFO, STATUS 等）

所有一般命令統一回應到 **CDC 介面**，便於集中監控和除錯：

| 命令來源 | CDC 回應 | HID 回應 | BLE 回應 | 說明 |
|---------|---------|---------|---------|------|
| CDC     | ✓ 是    | ✗ 否    | ✗ 否    | 僅回應到 CDC |
| HID     | ✓ 是    | ✗ 否    | ✗ 否    | 僅回應到 CDC |
| BLE     | ✓ 是    | ✗ 否    | ✗ 否    | 僅回應到 CDC |

#### SCPI 命令（*IDN? 等）

SCPI 命令回應到其**命令來源介面**，符合 SCPI 標準行為：

| 命令來源 | CDC 回應 | HID 回應 | BLE 回應 | 說明 |
|---------|---------|---------|---------|------|
| CDC     | ✓ 是    | ✗ 否    | ✗ 否    | 僅回應到命令來源 |
| HID     | ✗ 否    | ✓ 是    | ✗ 否    | 僅回應到命令來源 |
| BLE     | ✗ 否    | ✗ 否    | ✓ 是    | 僅回應到命令來源 |

**設計理由**：
- **一般命令**：集中到 CDC 便於統一除錯和監控所有介面的活動
- **SCPI 命令**：保持標準 SCPI 行為，回應到查詢來源介面

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
  # Prefer SDK config values (set CONFIG_TINYUSB_HID_BUFSIZE in sdkconfig.defaults)
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

## 🔌 硬體接腳定義

### 馬達控制和狀態顯示

| 功能 | GPIO | 週邊 | 說明 |
|------|------|------|------|
| PWM 輸出 | 10 | MCPWM1_A | 馬達控制 PWM 訊號 |
| 轉速計輸入 | 11 | MCPWM0_CAP0 | 硬體捕捉 Tachometer 訊號 |
| 脈衝輸出 | 12 | Digital Out | PWM 變更通知 |
| 狀態 LED | 48 | RMT Channel 0 | WS2812 RGB LED |
| USB D- | 19 | USB OTG | 內建 USB |
| USB D+ | 20 | USB OTG | 內建 USB |

### 週邊控制接腳

| 功能 | GPIO | 週邊 | 說明 |
|------|------|------|------|
| UART1 TX | 17 (TX1) | UART1 TX | UART/PWM 多工輸出 |
| UART1 RX | 18 (RX1) | UART1 RX / MCPWM0_CAP1 | UART 輸入或 RPM 測量 |
| UART2 TX | 43 (TX2) | UART2 TX | 標準 UART 輸出 |
| UART2 RX | 44 (RX2) | UART2 RX | 標準 UART 輸入 |
| Buzzer | 21 | LEDC Channel 2 | 蜂鳴器 PWM 輸出 |
| LED PWM | 14 | LEDC Channel 3 | LED 亮度控制 |
| Relay | 13 | Digital Out | 繼電器控制輸出 |
| GPIO | 47 | Digital Out | 通用 GPIO 輸出 |
| User Key 1 | 1 | Digital In (Pull-up) | 使用者按鍵 1 (增加) |
| User Key 2 | 2 | Digital In (Pull-up) | 使用者按鍵 2 (減少) |
| User Key 3 | 42 | Digital In (Pull-up) | 使用者按鍵 3 (Enter) |

### 連接建議

**PWM 輸出（GPIO 10）：**
```
ESP32 GPIO10 ─── 1kΩ ───┬─── 馬達驅動器 PWM 輸入
                        └─── 100nF ─── GND
```

**轉速計輸入（GPIO 11）：**
```
ESP32 GPIO11 ──┬── 10kΩ ──── 3.3V (Pull-up)
               └── 100nF ──── GND (濾波)
Tachometer Signal ────────────┘
```

**RGB LED（GPIO 48）：**
```
ESP32 GPIO48 ─── 470Ω ─── WS2812 DIN
3.3V ────────────────────── WS2812 VCC
GND ─────────────────────── WS2812 GND
```

**蜂鳴器（GPIO 21）：**
```
ESP32 GPIO21 ─── 100Ω ───┬─── NPN 基極 (例如 2N2222)
                         │
              集極 ──── Buzzer+ ──── 5V
              射極 ──── GND
              Buzzer- ──── GND
```

**繼電器（GPIO 13）：**
```
ESP32 GPIO13 ─── 1kΩ ───┬─── NPN 基極
                        │
             集極 ──── Relay 線圈 ──── 5V
             射極 ──── GND
             二極體 (1N4148) 反向並聯於線圈
```

**UART1/UART2 連接：**
```
ESP32 TX1/TX2 ───── RX (目標裝置)
ESP32 RX1/RX2 ───── TX (目標裝置)
GND ──────────────── GND (共地)
```

**使用者按鍵（GPIO 1, 2, 42）：**
```
GPIO ──┬── 按鍵 ── GND
       └── 10kΩ ── 3.3V (內部上拉已啟用)
```

## 🎯 專案結構

```
composite_device_test/
├── src/
│   ├── main.cpp                    # 主程式（USB、WiFi、馬達、週邊初始化）
│   ├── CustomHID.h/cpp             # 64-byte 自訂 HID 類別
│   ├── CommandParser.h/cpp         # 統一命令解析器
│   ├── PeripheralCommands.cpp      # 週邊控制命令處理
│   ├── HIDProtocol.h/cpp           # HID 協定處理
│   ├── MotorControl.h/cpp          # PWM 和轉速計控制
│   ├── MotorSettings.h/cpp         # 馬達設定管理
│   ├── PeripheralManager.h/cpp     # 週邊統一管理器
│   ├── PeripheralSettings.h/cpp    # 週邊設定和 NVS 持久化
│   ├── PeripheralPins.h            # 週邊接腳定義
│   ├── UART1Multiplexer.h/cpp      # UART1 多工器（UART/PWM/RPM）
│   ├── UART2.h/cpp                 # UART2 通訊介面
│   ├── Buzzer.h/cpp                # 蜂鳴器 PWM 控制
│   ├── LEDPWM.h/cpp                # LED PWM 亮度控制
│   ├── Relay.h/cpp                 # 繼電器控制
│   ├── GPIOControl.h/cpp           # GPIO 輸出控制
│   ├── UserKeys.h/cpp              # 使用者按鍵管理
│   ├── StatusLED.h/cpp             # WS2812 RGB LED 狀態指示
│   ├── WebServer.h/cpp             # Web 伺服器主檔
│   ├── WebServer_Peripherals.cpp   # 週邊 REST API 端點
│   ├── WiFiSettings.h/cpp          # WiFi 設定管理
│   ├── WiFiManager.h/cpp           # WiFi 連線管理
│   └── ble_provisioning.h/cpp      # BLE WiFi 配置
├── data/
│   ├── index.html                  # Web 控制面板（馬達）
│   ├── peripherals.html            # 週邊控制介面
│   └── settings.html               # 系統設定介面
├── scripts/
│   ├── test_hid.py                 # HID 測試腳本
│   ├── test_cdc.py                 # CDC 測試腳本
│   ├── test_all.py                 # 整合測試腳本
│   └── ble_client.py               # BLE GATT 測試客戶端
├── requirements.txt                # Python 依賴套件清單
├── platformio.ini                  # PlatformIO 配置
├── README.md                       # 本文件（繁體中文）
├── PROTOCOL.md                     # HID 協定規格（繁體中文）
├── TESTING.md                      # 測試指南（繁體中文）
├── CLAUDE.md                       # AI 開發說明（英文）
├── IMPLEMENTATION_GUIDE.md         # 實作指南（英文）
├── MOTOR_INTEGRATION_PLAN.md       # 整合計畫（英文）
└── STATUS_LED_GUIDE.md             # LED 指南（英文）
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
xTaskCreatePinnedToCore(bleTask, "BLE_Task", 4096, NULL, 1, NULL, 1);  // 優先權 = 1
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

# Scan for available BLE devices
python scripts/ble_client.py --scan

# Connect by device name
python scripts/ble_client.py --name ESP32_S3_Console

# Connect by address (if known)
python scripts/ble_client.py --address XX:XX:XX:XX:XX:XX
```

See `scripts/ble_client.py` for details and options.

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
