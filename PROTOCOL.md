# 多介面命令協定規格

## 概述

本文件定義了 ESP32-S3 多介面裝置的命令協定格式，涵蓋三種通訊介面：
- **USB CDC (Serial)**：純文字命令介面
- **USB HID (64-byte)**：雙協定命令介面（0xA1 結構化 / 純文字）
- **BLE GATT (Wireless)**：無線文字命令介面

所有介面使用統一的命令集和解析器，支援命令封包與原始資料的區分處理。

## 快速參考

### 關鍵特性

✅ **多介面支援**：USB CDC、USB HID、BLE GATT 三種通訊介面
✅ **無字元回顯**：CDC 輸入時不會顯示輸入字元，只顯示命令回應
✅ **統一 CDC 輸出**：除 SCPI 命令外，所有命令統一回應到 CDC（便於監控和除錯）
✅ **SCPI 例外**：SCPI 命令（如 *IDN?）回應到命令來源介面
✅ **命令封包識別**：HID 封包以 `0xA1` 開頭表示命令，否則為原始資料
✅ **自動分割**：長回應自動分割為多個 64-byte HID 封包
✅ **非同步架構**：HID 和 BLE 使用 FreeRTOS Task + Queue，避免 reentrant 問題
✅ **執行緒安全**：所有介面存取都有 FreeRTOS mutex 保護

### 命令格式對照

| 介面 | 發送格式 | 接收格式 |
|------|---------|---------|
| **CDC** | `HELP\n` | `可用命令:\n  *IDN?...\n` |
| **HID** | `[0xA1][0x05][0x00]HELP\n[...]` | `[0xA1][len][0x00]可用命令:...` |
| **BLE** | `HELP\n` (Write to RX) | `可用命令:...` (Notify from TX) |

### 回應路由規則

**設計原則：統一 CDC 輸出**

為了便於監控和除錯，除了 SCPI 命令外，所有命令都統一回應到 CDC 介面。

```
CDC 來源 → 一般命令 → CDC only
         → SCPI 命令 → CDC only

HID 來源 → 一般命令 → CDC only
         → SCPI 命令 → HID only (例外)

BLE 來源 → 一般命令 → CDC only
         → SCPI 命令 → BLE only (例外)
```

**回應路由表：**

**一般命令（HELP, INFO, STATUS 等）：**

| 命令來源 | CDC 回應 | HID 回應 | BLE 回應 | 說明 |
|---------|---------|---------|---------|------|
| CDC     | ✓       | ✗       | ✗       | 僅回應到 CDC |
| HID     | ✓       | ✗       | ✗       | 僅回應到 CDC |
| BLE     | ✓       | ✗       | ✗       | 僅回應到 CDC |

**SCPI 命令（*IDN? 等）：**

| 命令來源 | CDC 回應 | HID 回應 | BLE 回應 | 說明 |
|---------|---------|---------|---------|------|
| CDC     | ✓       | ✗       | ✗       | 僅回應到命令來源 |
| HID     | ✗       | ✓       | ✗       | 僅回應到命令來源 |
| BLE     | ✗       | ✗       | ✓       | 僅回應到命令來源 |

## HID 封包類型

### 1. 命令封包（Command Packet）

**識別標誌：** 首 byte 為 `0xA1`

**格式：**
```
[0xA1][length][0x00][command_string...]
```

**欄位說明：**
- **Byte 0**: `0xA1` - 命令封包識別碼
- **Byte 1**: `length` - 命令字串長度（1-61 bytes）
- **Byte 2**: `0x00` - 保留位元（協定擴充用）
- **Byte 3-63**: 命令字串（最多 61 bytes）

**限制：**
- Header 長度：3 bytes（固定）
- 命令字串最大長度：61 bytes
- 總封包長度：64 bytes（固定）

**範例：**
```
發送命令 "HELP\n" (5 bytes):
[0xA1][0x05][0x00]['H']['E']['L']['P']['\n'][0x00]...[0x00]
 ^^^   ^^^   ^^^   ^^^^^^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^
 類型  長度  保留         命令字串              補零到64bytes
```

### 2. 原始資料封包（Raw Data Packet）

**識別標誌：** 首 byte **不是** `0xA1`

**格式：**
```
[任意 64 bytes 資料]
```

**處理方式：**
- 存入 `hid_out_buffer[64]`（全域緩衝區）
- 設定 `hid_data_ready = true` 旗標
- 可使用 `READ` 命令查看緩衝區內容
- 可使用 `CLEAR` 命令清除緩衝區

**使用情境：**
- 測試 HID 傳輸（例如 `SEND` 命令發送 0x00-0x3F）
- 應用層資料傳輸（未來擴充）
- 非文字命令的二進位資料

## CDC 命令格式

### 接收格式

**格式：**
```
command_string\n
```

**特性：**
- 無 header，純文字命令
- 必須以 `\n` 或 `\r`（換行符）結尾
- **無字元回顯**：輸入字元不會顯示在終端
- 支援退格鍵（`\b` 或 ASCII 127），但無視覺回饋
- 只顯示命令回應，不顯示命令本身

**範例：**
```
輸入：HELP\n
輸出：（只顯示說明文字，不顯示 "HELP"）

輸入：*IDN?\n
輸出：HID_ESP32_S3（不包含 "*IDN?"）
```

### 回應格式

**格式：**
```
response_text
```

**特性：**
- 無 header，純文字輸出
- 直接輸出到 USB Serial (USBSerial)

## BLE GATT 協定

### 概述

ESP32-S3 提供 BLE (Bluetooth Low Energy) GATT 介面作為無線命令通道。

**重要限制：**
- ESP32-S3 **僅支援 BLE**（Bluetooth Low Energy）
- **不支援** Classic Bluetooth (BR/EDR)
- **不支援** Bluetooth Serial Port Profile (SPP)

### GATT 服務架構

**Service 定義：**
```
Service UUID:        4fafc201-1fb5-459e-8fcc-c5c9c331914b
  └─ TX Characteristic (Notify)
     UUID:           beb5483e-36e1-4688-b7f5-ea07361b26a9
     Properties:     Read, Notify
     用途:           裝置 → 客戶端（回應資料）

  └─ RX Characteristic (Write)
     UUID:           beb5483e-36e1-4688-b7f5-ea07361b26a8
     Properties:     Write, Write Without Response
     用途:           客戶端 → 裝置（命令輸入）
```

**裝置名稱：** `BillCat_Fan_Control`

### 命令發送格式

**透過 RX Characteristic 發送命令：**

**格式：**
```
command_string\n
```

**特性：**
- 純文字命令，無 header
- 必須以換行符結尾（`\n`）
- 最大長度：255 bytes（受 BLECommandPacket 限制）
- 使用 Write 或 Write Without Response 皆可

**範例（使用 Python bleak）：**
```python
import asyncio
from bleak import BleakClient

RX_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

async def send_command(address, command):
    async with BleakClient(address) as client:
        # 發送命令（必須加換行符）
        await client.write_gatt_char(RX_UUID, f"{command}\n".encode())
```

### 回應接收格式

**透過 TX Characteristic 接收回應：**

**格式：**
```
response_text
```

**特性：**
- 純文字回應，無 header
- 透過 BLE Notify 機制推送
- 長回應可能分割為多個 notification
- 最大單次 notification：512 bytes（取決於 BLE MTU）

**範例（使用 Python bleak）：**
```python
TX_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9"

def notification_handler(sender, data):
    response = data.decode('utf-8')
    print(f"回應: {response}")

async def receive_responses(address):
    async with BleakClient(address) as client:
        # 訂閱 TX Characteristic
        await client.start_notify(TX_UUID, notification_handler)
        await asyncio.sleep(30)  # 保持連線 30 秒
        await client.stop_notify(TX_UUID)
```

### 連線管理

**連線流程：**
1. 掃描 BLE 裝置（尋找 "BillCat_Fan_Control"）
2. 連線到裝置
3. 發現 Service 和 Characteristics
4. 訂閱 TX Characteristic（啟用 Notify）
5. 寫入命令到 RX Characteristic
6. 接收 TX Characteristic 的 Notify 回應

**自動重新廣播：**
- 裝置斷線後會自動重新開始廣播
- 支援多次連線/斷線循環

**連線狀態追蹤：**
```cpp
bool bleDeviceConnected = false;  // 全域連線狀態旗標
```

### BLE 非同步架構

**重要設計決策：避免 Reentrant 問題**

BLE RX Characteristic 的 `onWrite()` callback 不直接處理命令，而是將命令加入佇列：

**資料流程：**
```
BLE Client 寫入 RX Characteristic
    ↓
onWrite() callback (BLE stack context)
    ↓
xQueueSend(bleCommandQueue)  ← 只入佇列，不呼叫 notify
    ↓
bleTask (FreeRTOS Task context)
    ↓
xQueueReceive(bleCommandQueue)
    ↓
processCommand(multi_response)
    ↓
BLEResponse::println()
    ↓
pTxCharacteristic->notify()  ← 在 Task 上下文呼叫
```

**架構優勢：**
- ✅ 避免在 BLE callback 中呼叫 `notify()`（會導致 reentrant 錯誤）
- ✅ 與 HID Task 架構一致（ISR → Queue → Task）
- ✅ 符合 FreeRTOS 和 BLE stack 的最佳實踐
- ✅ 執行緒安全，可靠性高

### 測試工具

**Python 測試客戶端：** `scripts/ble_client.py`

**安裝依賴：**
```bash
pip install bleak
```

**使用方式：**
```bash
# 掃描所有 BLE 裝置
python scripts/ble_client.py --scan

# 通過名稱連線
python scripts/ble_client.py --name BillCat_Fan_Control

# 通過 MAC 位址連線
python scripts/ble_client.py --address XX:XX:XX:XX:XX:XX
```

**行動裝置測試：**
- 使用 nRF Connect app (Android/iOS)
- 連線到 "BillCat_Fan_Control"
- 啟用 TX Characteristic 的 Notify
- 寫入命令到 RX Characteristic（記得加 `\n`）

### BLE vs CDC vs HID 比較

| 特性 | CDC | HID | BLE |
|------|-----|-----|-----|
| 連線方式 | USB 有線 | USB 有線 | 無線 |
| 格式 | 純文字 | 雙協定（0xA1/純文字） | 純文字 |
| 最大長度 | 無限制 | 64 bytes/packet | 255 bytes/command |
| 回應方式 | 串流 | 封包 | Notify |
| 適用場景 | 開發除錯 | 應用程式整合 | 行動裝置、無線控制 |

## 統一命令系統

### 命令來源

系統支援三種命令來源：
1. **CDC** - USB 序列埠（Serial Console）
2. **HID** - 帶有 `0xA1` header 或純文字的命令封包
3. **BLE** - BLE GATT RX Characteristic（純文字命令）

### 命令處理流程

```
┌─────────────────┐   ┌──────────────────┐   ┌──────────────────┐
│  CDC Console    │   │  HID (0xA1)      │   │   BLE GATT       │
│  "HELP\n"       │   │  [A1][05][00]    │   │  "HELP\n"        │
│  (無回顯輸入)    │   │  [HELP\n]        │   │  (RX Char)       │
└────────┬────────┘   └────────┬─────────┘   └────────┬─────────┘
         │                     │                       │
         ▼                     ▼                       ▼
   ┌─────────────┐   ┌─────────────────┐   ┌─────────────────┐
   │  cdcTask    │   │    hidTask      │   │    bleTask      │
   │  (Core 1)   │   │    (Core 1)     │   │    (Core 1)     │
   └──────┬──────┘   └────────┬────────┘   └────────┬────────┘
          │                   │                       │
          │  feedChar()       │  isCommandPacket()    │  Queue Receive
          │  (累積字元)        │  (解析封包)            │  (非同步)
          │                   │                       │
          ▼                   ▼                       ▼
   ┌──────────────────────────────────────────────────────────┐
   │               CommandParser::processCommand               │
   │                   (統一命令處理邏輯)                        │
   │                                                            │
   │  判斷命令類型：                                              │
   │  - SCPI (*IDN?) → 回應到來源介面                            │
   │  - 其他命令 → 統一回應到 CDC                                │
   └──────┬───────────────────────┬──────────────────┬────────┘
          │                       │                  │
          │ CDCResponse           │ CDCResponse      │ CDCResponse
          │ (CDC only)            │ (CDC only)       │ (CDC only)
          │                       │ *IDN?例外:       │ *IDN?例外:
          │                       │ HIDResponse      │ BLEResponse
          ▼                       ▼                  ▼
   ┌─────────────┐   ┌─────────────────────┐   ┌──────────────────┐
   │     CDC     │   │       CDC           │   │       CDC        │
   │   純文字     │   │  (一般命令)          │   │  (一般命令)       │
   │             │   │                     │   │                  │
   │             │   │  *IDN? → HID        │   │  *IDN? → BLE     │
   │             │   │      (0xA1封包)      │   │      (Notify)    │
   └─────────────┘   └─────────────────────┘   └──────────────────┘

   說明：
   - CDC 來源命令 → 只回應到 CDC
   - HID 來源命令 → 一般命令回應到 CDC，SCPI 命令回應到 HID
   - BLE 來源命令 → 一般命令回應到 CDC，SCPI 命令回應到 BLE
```

### 回應機制

**回應路由取決於命令類型和來源：**

#### 1. CDC 來源命令（統一輸出到 CDC）

使用 **CDCResponse** 類別：

- **格式**：純文字 + 換行符
- **目標**：僅 USBSerial（CDC 介面）
- **所有命令**（包含 SCPI）都回應到 CDC
- **範例**：
  ```
  輸入：*IDN?\n
  CDC 輸出：HID_ESP32_S3\n
  HID 輸出：（無）
  BLE 輸出：（無）

  輸入：HELP\n
  CDC 輸出：可用命令...\n
  HID 輸出：（無）
  BLE 輸出：（無）
  ```

#### 2. HID 來源命令（依命令類型路由）

**一般命令**（HELP, INFO, STATUS 等）：

使用 **CDCResponse** 類別：
- **格式**：純文字 + 換行符
- **目標**：僅 CDC
- **範例**：
  ```
  輸入：HELP\n
  CDC 輸出：可用命令...\n
  HID 輸出：（無）
  ```

**SCPI 命令**（*IDN? 等）：

使用 **HIDResponse** 類別：
- **格式**：`[0xA1][length][0x00][response_text][padding...]`
- **目標**：僅 HID
- **自動分割**：長回應會分成多個 64-byte 封包
- **每個封包**：3 bytes header + 最多 61 bytes 資料
- **範例**：
  ```
  輸入：*IDN?\n
  CDC 輸出：（無）
  HID 輸出：[0xA1][0x0C][0x00]['H']['I']['D']...（12 bytes data）

  回應 "HID_ESP32_S3" (12 bytes):
  [0xA1][0x0C][0x00]['H']['I']['D']['_']['E']['S']['P']['3']['2']['_']['S']['3'][0x00]...[0x00]
       ^^^^  ^^^^  ^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
       類型  長度  保留                    資料 (12 bytes)                    補零到64
  ```

#### 3. BLE 來源命令（依命令類型路由）

**一般命令**（HELP, INFO, STATUS 等）：

使用 **CDCResponse** 類別：
- **格式**：純文字 + 換行符
- **目標**：僅 CDC
- **範例**：
  ```
  輸入（寫入 RX Char）：HELP\n
  CDC 輸出：可用命令...\n
  BLE 輸出（TX Char Notify）：（無）
  ```

**SCPI 命令**（*IDN? 等）：

使用 **BLEResponse** 類別：
- **格式**：純文字（無 header）
- **目標**：僅 BLE
- **傳輸**：透過 TX Characteristic Notify
- **長回應**：可能分割為多個 notification（取決於 BLE MTU）
- **範例**：
  ```
  輸入（寫入 RX Char）：*IDN?\n
  CDC 輸出：（無）
  BLE 輸出（TX Char Notify）：HID_ESP32_S3
  ```

#### 回應路由總表

**一般命令（HELP, INFO, STATUS, SEND, READ, CLEAR）：**

| 命令來源 | CDC 回應 | HID 回應 | BLE 回應 | 使用類別 |
|---------|---------|---------|---------|---------|
| CDC     | ✓ 是    | ✗ 否    | ✗ 否    | CDCResponse |
| HID     | ✓ 是    | ✗ 否    | ✗ 否    | CDCResponse |
| BLE     | ✓ 是    | ✗ 否    | ✗ 否    | CDCResponse |

**SCPI 命令（*IDN? 等）：**

| 命令來源 | CDC 回應 | HID 回應 | BLE 回應 | 使用類別 |
|---------|---------|---------|---------|---------|
| CDC     | ✓ 是    | ✗ 否    | ✗ 否    | CDCResponse |
| HID     | ✗ 否    | ✓ 是    | ✗ 否    | HIDResponse |
| BLE     | ✗ 否    | ✗ 否    | ✓ 是    | BLEResponse |

**設計理由：**
- 一般命令統一輸出到 CDC，便於監控和除錯
- SCPI 命令回應到來源介面，符合 SCPI 標準和應用程式整合需求

### 可用命令列表

| 命令 | 說明 | 回應內容 | 備註 |
|------|------|---------|------|
| `*IDN?` | SCPI 標準識別 | `HID_ESP32_S3` | 只回傳裝置 ID，不回顯命令 |
| `HELP` | 顯示命令列表 | 可用命令清單（中文） | 列出所有命令及說明 |
| `INFO` | 裝置資訊 | 型號、CDC/HID 狀態、記憶體 | 完整設備資訊 |
| `STATUS` | 系統狀態 | 運行時間、記憶體、HID 資料狀態 | 即時系統狀態 |
| `SEND` | 發送測試資料 | 成功/失敗訊息 | HID IN 報告（0x00-0x3F 序列） |
| `READ` | 讀取 HID 緩衝區 | Hex dump (64 bytes) | 顯示 `hid_out_buffer` 內容 |
| `CLEAR` | 清除 HID 緩衝區 | 確認訊息 | 清空緩衝區和 `hid_data_ready` 旗標 |

**命令特性：**
- 所有命令不區分大小寫（`help` = `HELP` = `HeLp`）
- 必須以換行符結尾（`\n` 或 `\r`）
- 輸入時無回顯（silent input）
- 未知命令會顯示錯誤訊息並提示輸入 `HELP`

## 實作細節

### FreeRTOS 架構

**Task 分工：**
- **hidTask** (Priority 2, Core 1)：
  - 從 `hidDataQueue` 接收 HID OUT 封包（佇列由 ISR 填充）
  - 使用 `HIDProtocol::isCommandPacket()` 判斷封包類型
  - **命令封包** (0xA1 header) → 根據命令類型路由回應：
    - SCPI 命令 → 使用 `HIDResponse`（僅 HID 回應）
    - 一般命令 → 使用 `CDCResponse`（僅 CDC 回應）
  - **原始資料** → 存入 `hid_out_buffer`，顯示除錯資訊

- **cdcTask** (Priority 1, Core 1)：
  - 輪詢 CDC 序列輸入（`USBSerial.available()`）
  - 使用 `feedChar()` 逐字元累積命令
  - **無字元回顯**：輸入字元不輸出到終端
  - 支援退格鍵（修改緩衝區但無視覺回饋）
  - 遇到 `\n` 或 `\r` → 使用 `CDCResponse` 執行命令

- **bleTask** (Priority 1, Core 1)：
  - 從 `bleCommandQueue` 接收 BLE 命令（佇列由 BLE RX callback 填充）
  - **在 Task 上下文處理命令**（避免在 BLE callback 中呼叫 notify）
  - 根據命令類型路由回應：
    - SCPI 命令 → 使用 `BLEResponse`（僅 BLE 回應）
    - 一般命令 → 使用 `CDCResponse`（僅 CDC 回應）
  - 非同步架構與 hidTask 一致，符合 FreeRTOS 最佳實踐

**同步機制：**
- `hidSendMutex`：保護 `HID.send()` 呼叫（防止多 task 同時傳送）
- `serialMutex`：保護 `USBSerial` 存取（cdcTask、hidTask 和 bleTask 都會輸出到 CDC）
- `bufferMutex`：保護 `hid_out_buffer` 存取（hidTask 寫入，READ 命令讀取）
- `hidDataQueue`：ISR → hidTask 的資料傳遞佇列（深度 10，非阻塞發送）
- `bleCommandQueue`：BLE RX callback → bleTask 的命令傳遞佇列（深度 10）

**資料流程：**
```
ISR 上下文:
  onHIDData() → xQueueSendFromISR(hidDataQueue) → portYIELD_FROM_ISR()

hidTask 上下文:
  xQueueReceive() → isCommandPacket() → processCommand(multi_response)
                                     → 或存入 hid_out_buffer

cdcTask 上下文:
  USBSerial.read() → feedChar() → processCommand(cdc_response)

BLE RX Callback 上下文:
  onWrite() → xQueueSend(bleCommandQueue)  (僅入佇列，不呼叫 notify)

bleTask 上下文:
  xQueueReceive(bleCommandQueue) → processCommand(multi_response)
  → BLEResponse::println() → pTxCharacteristic->notify()
```

### 協定處理類別

**HIDProtocol 類別：** (`src/HIDProtocol.h/cpp`)

```cpp
// 檢查是否為命令封包
bool HIDProtocol::isCommandPacket(
    const uint8_t* data,        // 輸入：64-byte HID 封包
    char* command_out,          // 輸出：命令字串緩衝區（至少 62 bytes）
    uint8_t* command_len_out    // 輸出：命令長度
);

// 編碼回應封包（加上 3-byte header）
uint8_t HIDProtocol::encodeResponse(
    uint8_t* out,               // 輸出：64-byte 緩衝區
    const uint8_t* payload,     // 輸入：實際資料
    uint8_t payload_len         // 輸入：資料長度（最多 61 bytes）
);
```

### 回應類別架構

**介面與實作：** (`src/CommandParser.h`)

```cpp
// 抽象介面
class ICommandResponse {
    virtual void print(const char* str) = 0;
    virtual void println(const char* str) = 0;
    virtual void printf(const char* format, ...) = 0;
};

// CDC 專用回應（純文字輸出）
class CDCResponse : public ICommandResponse {
    // 輸出到 USBSerial
};

// HID 專用回應（帶 0xA1 header）
class HIDResponse : public ICommandResponse {
    // 自動加上 header，分割長文字為多個 64-byte 封包
    // 每個封包之間延遲 10ms 確保主機接收
};

// 多通道回應（CDC + HID）
class MultiChannelResponse : public ICommandResponse {
    // 同時包裝 CDCResponse 和 HIDResponse
    // 所有輸出自動轉發到兩個通道
};
```

**使用方式：**
```cpp
// setup() 中建立回應物件
cdc_response = new CDCResponse(USBSerial);
hid_response = new HIDResponse(&HID);
multi_response = new MultiChannelResponse(cdc_response, hid_response);

// cdcTask 中（CDC 來源命令）
parser.feedChar(c, buffer, cdc_response, CMD_SOURCE_CDC);
// → 結果：只輸出到 CDC

// hidTask 中（HID 來源命令）
// SCPI 命令 → 只回應到 HID
// 一般命令 → 只回應到 CDC
if (CommandParser::isSCPICommand(cmd)) {
    parser.processCommand(cmd, hid_response, CMD_SOURCE_HID);
} else {
    parser.processCommand(cmd, cdc_response, CMD_SOURCE_HID);
}

// bleTask 中（BLE 來源命令）
// SCPI 命令 → 只回應到 BLE
// 一般命令 → 只回應到 CDC
if (CommandParser::isSCPICommand(cmd)) {
    parser.processCommand(cmd, ble_response, CMD_SOURCE_BLE);
} else {
    parser.processCommand(cmd, cdc_response, CMD_SOURCE_BLE);
}
```

## 主機端測試範例

### Python (pywinusb)

**發送命令：**
```python
import pywinusb.hid as hid

# 開啟裝置（VID=0x303A, PID=0x4002）
device = hid.HidDeviceFilter(vendor_id=0x303A, product_id=0x4002).get_devices()[0]
device.open()

# 準備命令 "HELP\n" (5 bytes)
cmd = "HELP\n"
length = len(cmd)
data = [0xA1, length, 0x00] + [ord(c) for c in cmd] + [0] * (61 - length)

# 發送（HID output report 不需要 Report ID prefix，因為我們使用無 Report ID 的描述符）
output_report = device.find_output_reports()[0]
output_report.set_raw_data([0] + data)  # [0] 表示無 Report ID
output_report.send()
```

**接收回應：**
```python
def on_data(data):
    # data[0] = Report ID (0)
    # data[1] = 0xA1 (命令回應)
    # data[2] = length
    # data[3] = 0x00
    # data[4:4+length] = 回應文字

    if data[1] == 0xA1:
        length = data[2]
        response = ''.join(chr(b) for b in data[4:4+length])
        print(f"HID Response: {response}")

device.set_raw_data_handler(on_data)
```

### hidapitester (命令列工具)

**發送命令：**
```bash
# 發送 "INFO\n" 命令
hidapitester --vidpid 303A:4002 --open --send-output 0xA1,0x05,0x00,0x49,0x4E,0x46,0x4F,0x0A,0x00,...
#                                                      ^^^^  ^^^^  ^^^^  ^^^^^^^^^^^^^^^^^^^  ^^^^
#                                                      類型  長度  保留   "INFO\n" (ASCII)    補零
```

## 錯誤處理

### 無效命令封包

以下情況會將封包視為**原始資料**（而非命令）：

1. **首 byte 不是 0xA1**
2. **長度欄位為 0 或 > 61**
3. **保留位元不是 0x00**

### Mutex 逾時

- `serialMutex`、`bufferMutex`、`hidSendMutex` 的逾時時間：100ms
- 如果取得 mutex 失敗，操作會被跳過（不會阻塞）

### Queue 滿

- `hidDataQueue` 容量：10 個封包
- 如果佇列滿，ISR 會丟棄新資料（優先保留舊資料）

## 常見問題排除

### Q1: 為什麼輸入命令時看不到字元？

**答**：這是設計行為。系統已移除字元回顯以提供乾淨的輸出。
- **CDC 模式**：輸入 `HELP\n` 只會顯示說明文字，不會顯示 `HELP`
- **如需重新啟用 echo**：修改 `CommandParser.cpp` 中的 `feedChar()` 函數

### Q2: CDC 命令為什麼沒有 HID 回應？

**答**：這是設計行為。CDC 命令只回應到 CDC 介面。
- **CDC → CDC only**（節省 HID 頻寬）
- **HID → CDC + HID**（方便除錯和監控）

### Q3: HID 回應為什麼有 3 bytes header？

**答**：Header 格式 `[0xA1][length][0x00]` 用於：
- **區分命令和原始資料**：0xA1 = 命令/回應
- **指示資料長度**：主機可正確解析可變長度回應
- **協定擴充**：保留位元可用於未來功能

### Q4: 長回應（如 HELP）如何處理？

**答**：HIDResponse 自動處理：
- 將文字分割為最多 61-byte 的區塊
- 每個區塊加上 3-byte header 組成 64-byte 封包
- 封包之間延遲 10ms 確保主機接收
- 主機端需要組合多個封包還原完整文字

### Q5: 如何判斷 mutex 是否造成問題？

**答**：檢查以下症狀：
- **回應遺失**：可能是 `serialMutex` 或 `hidSendMutex` 逾時
- **資料不完整**：可能是 `bufferMutex` 競爭
- **建議**：在 mutex 取得失敗處添加計數器或 LED 指示

## 未來擴充方向

### 協定擴充

- **TYPE_RESPONSE (0xA2)**：專用回應標誌（與命令區分）
- **TYPE_DATA (0xA0)**：應用層資料傳輸
- **CRC/Checksum**：資料完整性檢查
- **序號/時間戳記**：封包追蹤

### 命令系統擴充

- **命令註冊表** (`CommandRegistry`)：動態命令註冊
- **權限管理**：命令存取控制
- **巨集命令**：批次命令執行
- **事件訂閱**：主動推送通知

### 多通道擴充

- **BLE 介面**：藍牙命令來源
- **WebSocket**：網頁控制介面
- **選擇性輸出**：根據命令來源決定回應目標

## 版本歷史

### v2.2 (目前版本) - 2025-01
- ✅ **重構回應路由機制**：實現基於命令類型的智慧路由
  - 一般命令（HELP, INFO, STATUS 等）→ 所有來源統一回應到 CDC
  - SCPI 命令（*IDN? 等）→ 回應到命令來源介面
- ✅ **新增 isSCPICommand() 方法**：自動偵測 SCPI 命令（以 * 開頭）
- ✅ **符合協定規格**：完全符合 PROTOCOL.md 回應路由表
- ✅ **改進除錯體驗**：所有介面的一般命令統一在 CDC 顯示，便於監控

### v2.1 - 2025-01
- ✅ **移除字元回顯**：CDC 輸入無 echo，實現乾淨的命令回應
- ✅ **智慧回應路由**：
  - CDC 命令 → 僅 CDC 回應（CDCResponse）
  - HID 命令 → CDC + HID 雙通道回應（MultiChannelResponse）
- ✅ **命令回應優化**：`*IDN?` 只返回設備 ID，不包含命令本身
- ✅ **文檔完善**：更新所有流程圖和說明反映實際行為

### v2.0 - 2024-12
- ✅ 實作 0xA1 header 檢測
- ✅ HID 回應加上 3-byte header
- ✅ 多通道回應系統（MultiChannelResponse）
- ✅ 命令與原始資料分流處理
- ✅ FreeRTOS mutex 保護 HID.send()

### v1.0 - 2024-11
- 簡單字串解析（逐字元掃描 `\n`）
- HID 回應無 header
- 回應只傳送到來源介面
- 所有介面都有字元回顯

---

## 文檔變更總結 (v2.1)

本次更新完整反映了命令解析器的實際行為：

### 主要更新

1. **CDC 輸入行為**
   - ✅ 明確說明無字元回顯特性
   - ✅ 更新範例顯示只有回應輸出
   - ✅ 說明如何重新啟用 echo

2. **回應路由機制**
   - ✅ 新增詳細的流程圖（區分 CDC/HID 路徑）
   - ✅ 新增回應路由表格
   - ✅ 明確標示使用的回應類別

3. **命令列表優化**
   - ✅ 擴充為 4 欄表格（命令/說明/回應/備註）
   - ✅ 突出 `*IDN?` 無回顯特性
   - ✅ 添加命令特性清單

4. **架構說明強化**
   - ✅ FreeRTOS Task 分工更詳細
   - ✅ 數據流程圖更新
   - ✅ 回應類別架構完整說明

5. **新增實用內容**
   - ✅ 快速參考區段（關鍵特性一覽）
   - ✅ 常見問題排除（5 個 FAQ）
   - ✅ 版本歷史追蹤

### 文檔品質改善

- 📊 **視覺化**：新增多個流程圖和表格
- 🎯 **準確性**：所有內容與實際程式碼一致
- 📖 **易讀性**：結構化組織，快速查找
- 🔍 **完整性**：涵蓋設計決策和實作細節
