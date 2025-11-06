# 測試指南

本文件提供 ESP32-S3 USB 複合裝置的完整測試說明，包含測試腳本使用、故障排除和進階測試。

## 📋 目錄

- [準備工作](#準備工作)
- [測試腳本概覽](#測試腳本概覽)
- [測試命令列表](#測試命令列表)
- [COM Port 智慧過濾](#com-port-智慧過濾)
- [測試場景](#測試場景)
- [驗證多通道功能](#驗證多通道功能)
- [協定說明](#協定說明)
- [故障排除](#故障排除)
- [進階測試](#進階測試)

## 準備工作

### 1. 安裝 Python 依賴

```bash
pip install -r requirements.txt
```

這會安裝：
- `pyserial` - CDC Serial 通訊
- `pywinusb` - HID 裝置通訊（Windows 專用）

### 2. 上傳韌體

**重要步驟：**

1. **進入 Bootloader 模式**：
   - 按住 BOOT 按鈕
   - 按下 RESET 按鈕
   - 放開 BOOT 按鈕

2. **上傳韌體**：
   ```bash
   pio run -t upload
   ```

3. **實體重新插拔 USB 線**（非常重要！）
   - 上傳完成後，拔掉 USB 線
   - 等待 2 秒
   - 重新插上 USB 線
   - 這確保 USB 裝置正確列舉

### 3. 驗證裝置

**Windows 裝置管理員檢查：**
- 連接埠 (COM 和 LPT) → 應該有新的 COM port（例如 COM3）
- 人機介面裝置 → 應該有 "TinyUSB HID"

**檢查 VID:PID：**
```bash
python -m serial.tools.list_ports -v
```

應該看到：
```
COMX
  描述: USB 序列裝置
  VID:PID: 303A:4002  ✓ 正確
```

## 測試腳本概覽

專案包含三個主要測試腳本：

### 1. test_hid.py - HID 介面測試工具

**功能：**
- 支援兩種協定：純文字和 0xA1 協定
- 列出所有 HID 裝置
- 發送命令並接收回應
- 發送原始十六進位資料
- 接收資料模式
- 互動模式（支援動態切換協定）

**使用方式：**
```bash
# 顯示說明
python test_hid.py

# 列出所有 HID 裝置
python test_hid.py list

# 發送單個命令（純文字協定）
python test_hid.py cmd *IDN?

# 測試所有命令（純文字協定）
python test_hid.py test

# 測試所有命令（0xA1 協定）
python test_hid.py test-0xa1

# 發送原始資料
python test_hid.py send 12 34 56 78

# 互動模式（純文字協定）
python test_hid.py interactive

# 互動模式（0xA1 協定）
python test_hid.py interactive-0xa1

# 接收資料模式
python test_hid.py receive
```

**互動模式指令：**
- `<命令>` - 直接發送命令（例如：`*IDN?`）
- `cmd: <命令>` - 明確指定命令模式
- `hex: <資料>` - 發送十六進位資料（例如：`hex: 12 34 56`）
- `protocol` - 切換協定（純文字 ↔ 0xA1）
- `q`, `quit`, `exit` - 離開互動模式

**範例：**
```bash
$ python test_hid.py interactive-0xa1

=== HID 互動模式（0xA1 協定）===
輸入命令（或 'q' 離開，'protocol' 切換協定）：

>>> *IDN?
📤 發送命令: *IDN?
  封包 (前16): A1 06 00 2A 49 44 4E 3F 0A 00 00 00 00 00 00 00
📥 回應:
  HID_ESP32_S3

>>> protocol
✓ 已切換到純文字協定

>>> HELP
📤 發送命令: HELP
  封包 (前16): 48 45 4C 50 0A 00 00 00 00 00 00 00 00 00 00 00
📥 回應:
  可用命令:
    *IDN? - 裝置識別
    ...
```

### 2. test_cdc.py - CDC 介面測試工具

**功能：**
- 自動掃描並找到 ESP32-S3 的 CDC 序列埠
- 測試所有命令（*IDN?, INFO, STATUS, HELP）
- 互動模式（可輸入任意命令）
- 列出所有 COM ports 詳細資訊

**使用方式：**
```bash
# 自動掃描並測試（預設行為）
python test_cdc.py

# 列出所有 COM ports
python test_cdc.py list

# 僅測試命令
python test_cdc.py test

# 直接進入互動模式
python test_cdc.py interactive
```

**預期輸出：**
```
=== 掃描 COM Ports (只掃描 USB CDC 裝置) ===

嘗試 COM8...
  描述: USB Serial Device
  VID:PID: 303A:4002
  發送命令: *IDN?
  ✅ 找到裝置！回應: HID_ESP32_S3

=== 開始測試命令 ===
📤 測試命令: *IDN? (裝置識別)
📥 回應:
  HID_ESP32_S3

📤 測試命令: INFO (裝置資訊)
📥 回應:
  裝置資訊:
  晶片型號: ESP32-S3
  自由記憶體: 295632 bytes
  ...
```

### 3. test_all.py - 整合測試工具

**功能：**
- 同時測試 CDC 和 HID 介面
- 驗證多通道回應功能（HID 命令應同時在 CDC 和 HID 回應）
- 可選擇僅測試單一介面
- 自動比較 CDC 和 HID 的回應是否一致

**使用方式：**
```bash
# 測試 CDC 和 HID（預設）
python test_all.py

# 僅測試 CDC 介面
python test_all.py cdc

# 僅測試 HID 介面
python test_all.py hid

# 明確測試多通道回應
python test_all.py both
```

**預期輸出：**
```
==========================================================
ESP32-S3 整合測試工具
測試 CDC 和 HID 多通道回應功能
==========================================================

✅ pyserial 已安裝
✅ pywinusb 已安裝

=== 掃描 CDC (Serial) 介面 ===
嘗試 COM8... ✅ 找到！(HID_ESP32_S3)

=== 掃描 HID 介面 ===
✅ 找到 HID 裝置: TinyUSB HID

==========================================================
開始測試多通道回應
==========================================================

命令: *IDN?
----------------------------------------
📡 CDC 回應: HID_ESP32_S3
📡 HID 回應: HID_ESP32_S3
✅ 回應一致（多通道功能正常）

命令: INFO
----------------------------------------
📡 CDC 回應:
  裝置資訊:
  晶片型號: ESP32-S3
  ...
📡 HID 回應:
  裝置資訊:
  晶片型號: ESP32-S3
  ...
✅ 回應一致（多通道功能正常）

==========================================================
測試完成！
==========================================================

📊 測試總結:
  CDC 介面: ✅ 正常
  HID 介面: ✅ 正常
  多通道回應: ✅ 正常

✅ 所有介面正常運作
✅ 多通道回應功能已驗證
```

## 測試命令列表

所有測試腳本支援以下命令：

| 命令 | 描述 | 預期回應 | 備註 |
|------|------|---------|------|
| `*IDN?` | SCPI 標準識別命令 | `HID_ESP32_S3` | 只回傳裝置 ID |
| `HELP` | 顯示可用命令列表 | 命令列表（中文） | 列出所有命令及說明 |
| `INFO` | 顯示裝置資訊 | 晶片型號、記憶體資訊 | 完整設備資訊 |
| `STATUS` | 顯示系統狀態 | 運行時間、記憶體、HID 狀態 | 即時系統狀態 |
| `SEND` | 發送測試 HID IN report | 確認訊息 | 發送 0x00-0x3F 序列 |
| `READ` | 讀取 HID OUT buffer | Hex dump (64 bytes) | 顯示緩衝區內容 |
| `CLEAR` | 清除 HID OUT buffer | 確認訊息 | 清空緩衝區 |

**命令特性：**
- 所有命令不區分大小寫（`help` = `HELP` = `HeLp`）
- 必須以換行符結尾（`\n` 或 `\r`）
- 輸入時無回顯（silent input）
- 未知命令會顯示錯誤訊息並提示輸入 `HELP`

## COM Port 智慧過濾

### 為什麼需要過濾？

掃描所有 COM ports 時可能遇到的問題：
- **藍牙裝置**：連接緩慢或超時（可能需要數秒到十幾秒）
- **虛擬 COM ports**：不是真實的 USB 裝置，會浪費掃描時間
- **非 CDC 裝置**：印表機、數據機等非序列通訊裝置

### 過濾規則

測試腳本會自動跳過以下裝置：

**1. 藍牙裝置**（支援中英文）
- 檢測方式：描述中包含藍牙相關關鍵字
- 英文關鍵字：`bluetooth`, `bt `
- 中文關鍵字：`藍牙`, `藍芽`, `透過藍牙`, `透過藍芽`
- 範例：
  ```
  ⏭️  跳過 COM5 (藍牙裝置)
      描述: Standard Serial over Bluetooth link
  ⏭️  跳過 COM3 (藍牙裝置)
      描述: 透過藍牙連結的標準序列 (COM3)
  ```

**2. 虛擬 COM Ports**
- 檢測方式：沒有 VID/PID（不是真實的 USB 裝置）
- 範例：`⏭️  跳過 COM1 (虛擬 COM port，無 VID/PID)`
- 常見虛擬 ports：Windows 內建 COM1-COM4

**3. 非 CDC 裝置**（支援中英文）
- 檢測方式：描述中包含特定關鍵字
- 英文關鍵字：`printer`, `modem`, `dialup`, `irda`
- 中文關鍵字：`印表機`, `數據機`
- 範例：`⏭️  跳過 COM6 (非 CDC 裝置) - USB Printer Port`

### Timeout 機制

為避免掃描卡在無回應的裝置上，測試腳本採用多層 timeout 保護：

**連線階段：**
- 序列埠讀取 timeout：0.5 秒
- 序列埠寫入 timeout：1.0 秒
- 裝置穩定等待時間：0.5 秒

**命令回應階段：**
- 總 timeout 時間：2.0 秒
- 輪詢間隔：0.05 秒（避免過度消耗 CPU）
- 超時後顯示：`⏱️  無回應 (timeout 2.0s)`

**單個 COM port 最長測試時間：**
- 開啟序列埠 + 穩定等待 + 命令回應 ≈ 3 秒
- 確保掃描不會因單一裝置而長時間卡住

### 效能改善

| 場景 | 掃描時間（無過濾） | 掃描時間（有過濾） | 改善 |
|------|-------------------|-------------------|------|
| 只有 ESP32 | 3s | 3s | - |
| + 1 個藍牙裝置 | 15s | 3s | ⚡ 5x |
| + 2 個藍牙 + 虛擬 ports | 30s+ | 3s | ⚡ 10x+ |

### 掃描輸出範例

```
=== 掃描 COM Ports (只掃描 USB CDC 裝置) ===

⏭️  跳過 COM1 (虛擬 COM port，無 VID/PID)

⏭️  跳過 COM3 (藍牙裝置)
    描述: 透過藍牙連結的標準序列 (COM3)

⏭️  跳過 COM5 (藍牙裝置)
    描述: Standard Serial over Bluetooth link (COM5)

嘗試 COM8...
  描述: USB Serial Device
  VID:PID: 303A:4002
  開啟序列埠...
  等待裝置穩定...
  發送命令: *IDN?
  ✅ 找到裝置！回應: HID_ESP32_S3
```

## 測試場景

### 場景 1：快速驗證裝置功能

**目標**：一次測試所有介面

```bash
python test_all.py
```

### 場景 2：調試 HID 通訊

**目標**：互動式測試不同協定

```bash
# 使用互動模式，動態切換協定
python test_hid.py interactive

# 在互動模式中：
>>> *IDN?           # 測試純文字協定
>>> protocol        # 切換到 0xA1 協定
>>> *IDN?           # 測試 0xA1 協定
>>> hex: A1 05 00 48 45 4C 50 0A  # 發送原始資料
```

### 場景 3：調試 CDC 通訊

**目標**：互動式測試序列埠命令

```bash
python test_cdc.py interactive
```

### 場景 4：驗證多通道回應

**目標**：確認 HID 命令同時在 CDC 和 HID 回應

**方法 1：使用整合測試**
```bash
python test_all.py both
```

**方法 2：同時觀察兩個介面**

1. **終端機 1**：開啟 Serial Monitor
   ```bash
   pio device monitor --baud 115200 --echo --eol LF
   ```

2. **終端機 2**：執行 HID 測試
   ```bash
   python test_hid.py test-0xa1
   ```

3. **觀察結果**：
   - 當 HID 發送命令時
   - Serial Monitor 也應該顯示回應
   - 這驗證了「多通道回應」功能

## 驗證多通道功能

### 回應路由規則

| 命令來源 | CDC 回應 | HID 回應 | 說明 |
|---------|---------|---------|------|
| CDC     | ✓ 是    | ✗ 否    | 只回應到 CDC 介面 |
| HID     | ✓ 是    | ✓ 是    | 同時回應到 CDC 和 HID |

### 驗證步驟

**1. 測試 CDC 命令回應（僅 CDC）**

```bash
pio device monitor --baud 115200 --echo --eol LF
```

在 Serial Monitor 中輸入：
```
*IDN?
```

預期行為：
- CDC 介面顯示回應：`HID_ESP32_S3`
- HID 介面無回應（這是正確的）

**2. 測試 HID 命令回應（CDC + HID）**

終端機 1（Serial Monitor）：
```bash
pio device monitor --baud 115200 --echo --eol LF
```

終端機 2（HID 測試）：
```bash
python test_hid.py cmd INFO
```

預期行為：
- HID 測試腳本顯示 INFO 回應
- Serial Monitor **同時**顯示 INFO 回應
- 這驗證多通道功能正常

## 協定說明

### 純文字協定（簡單）

**OUT Report**（Host → Device）：
```
[命令文字]\n + 填充到 64 bytes
```

範例：
```
*IDN?\n[0x00]...[0x00]
```

**IN Report**（Device → Host）：
```
[回應文字] + 填充到 64 bytes
```

範例：
```
HID_ESP32_S3[0x00]...[0x00]
```

**適用於**：
- 簡單的文字命令
- Bus Hound 分析
- 快速測試

### 0xA1 協定（結構化）

**OUT Report**（Host → Device）：
```
[0xA1][length][0x00][命令文字] + 填充到 64 bytes
```

範例（發送 "HELP\n"）：
```
[0xA1][0x05][0x00]['H']['E']['L']['P']['\n'][0x00]...[0x00]
 ^^^   ^^^   ^^^   ^^^^^^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^
 類型  長度  保留         命令字串              補零
```

**IN Report**（Device → Host）：
```
[0xA1][length][0x00][回應文字] + 填充到 64 bytes
```

範例（回應 "OK"）：
```
[0xA1][0x02][0x00]['O']['K'][0x00]...[0x00]
```

**特點**：
- 支援多封包回應（長回應自動分割）
- 明確的長度欄位
- 保留位元供未來擴充

**適用於**：
- 複雜的結構化資料
- 應用程式整合
- 生產環境

詳細協定規格請參閱 [PROTOCOL.md](PROTOCOL.md)。

## 故障排除

### 問題 1: 找不到 COM port

**可能原因：**
- USB 線未正確連接
- 驅動程式未安裝
- DTR 未啟用（測試腳本已自動啟用）

**解決方法：**
1. 檢查 USB 線是否支援資料傳輸（不是只有充電線）
2. 重新插拔 USB 線
3. 檢查裝置管理員是否有未知裝置
4. 重新上傳韌體

**檢查命令：**
```bash
python -m serial.tools.list_ports -v
```

### 問題 2: 找不到 HID 裝置

**可能原因：**
- 韌體未正確上傳
- USB 未重新列舉
- VID:PID 不正確

**解決方法：**
1. **重新插拔 USB 線**（最重要！）
2. 檢查裝置管理員中的 "Human Interface Devices"
3. 確認有 "TinyUSB HID" 裝置
4. 檢查 VID:PID 是否為 303A:4002（不是 303A:1001）

**檢查命令：**
```bash
python test_hid.py list
```

### 問題 3: HID 回應為空

**可能原因：**
- 命令格式錯誤（純文字未加 `\n`）
- 使用了錯誤的協定
- 裝置未準備好

**驗證方法：**
1. 檢查 Serial Monitor 的除錯輸出
2. 確認使用正確的協定（test vs test-0xa1）
3. 嘗試使用 receive 模式查看是否有資料

**正確的命令封包（0xA1 協定）：**
```python
# HELP\n (5 bytes)
packet = [0xA1, 0x05, 0x00, ord('H'), ord('E'), ord('L'), ord('P'), ord('\n')] + [0]*56
```

### 問題 4: CDC 和 HID 回應不一致

**可能原因：**
- `multi_response` 未正確設置
- FreeRTOS Mutex 問題
- 韌體版本不匹配

**檢查點：**
- 確認 `main.cpp` 中 `multi_response` 已建立
- 確認 hidTask 使用 `multi_response`
- 檢查 Serial Monitor 的除錯訊息
- 重新編譯和上傳韌體

### 問題 5: 掃描速度很慢

**可能原因：**
- 未啟用 COM port 過濾
- 有多個藍牙裝置

**解決方法：**
- 測試腳本已自動啟用過濾
- 如需手動調整，修改腳本中的 `skip_keywords` 列表

## 進階測試

### 測試 1：原始資料封包

發送不帶 0xA1 header 的資料：

```python
import hid

h = hid.Device(vid=0x303A, pid=0x4002)

# 發送原始資料（首 byte 不是 0xA1）
raw_data = bytes([0x00, 0x01, 0x02, 0x03] + [0xFF]*60)
h.write(raw_data)

h.close()
```

在 Serial Monitor 應該看到：
```
[DEBUG] HID OUT (原始資料): 64 位元組
前16: 00 01 02 03 FF FF FF FF FF FF FF FF FF FF FF FF
後16: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF
```

### 測試 2：性能測試

連續發送多個命令：

```python
import time

for i in range(100):
    send_command(h, "STATUS")
    time.sleep(0.01)
```

觀察：
- 是否有丟包
- 回應延遲
- Serial Monitor 的 Task 統計

### 測試 3：壓力測試

**終端機 1**：持續發送 CDC 命令
```bash
while true; do echo "*IDN?" | nc localhost 8080; sleep 0.1; done
```

**終端機 2**：持續發送 HID 命令
```bash
python test_hid.py receive
```

觀察：
- 是否有 Mutex 衝突
- 記憶體洩漏
- Task 切換效能

### 測試 4：長回應測試

發送會產生長回應的命令：

```bash
python test_hid.py cmd HELP
```

觀察：
- 是否正確分割為多個 64-byte 封包
- 封包順序是否正確
- 是否有遺失封包

### 測試 5：協定切換測試

在互動模式中快速切換協定：

```bash
python test_hid.py interactive

>>> *IDN?       # 純文字
>>> protocol    # 切換
>>> *IDN?       # 0xA1
>>> protocol    # 切換
>>> *IDN?       # 純文字
```

## 測試檢查清單

執行完整測試時，確認以下項目：

### 硬體檢查
- [ ] USB 線支援資料傳輸
- [ ] ESP32-S3 正常供電
- [ ] 韌體已成功上傳
- [ ] USB 線已重新插拔

### 裝置管理員檢查
- [ ] 有新的 COM port 出現
- [ ] 有 "TinyUSB HID" 裝置
- [ ] VID:PID 為 303A:4002
- [ ] 沒有黃色驚嘆號

### CDC 介面測試
- [ ] CDC 介面可以找到裝置
- [ ] `*IDN?` 命令正常回應
- [ ] `INFO` 命令顯示裝置資訊
- [ ] `STATUS` 命令顯示系統狀態
- [ ] `HELP` 命令列出所有命令

### HID 介面測試
- [ ] HID 介面可以找到裝置
- [ ] 純文字協定測試通過
- [ ] 0xA1 協定測試通過
- [ ] HID 回應帶有 0xA1 header
- [ ] 長回應正確分割

### 多通道功能測試
- [ ] HID 命令在 CDC 介面有回應
- [ ] HID 命令在 HID 介面有回應
- [ ] CDC 和 HID 的回應內容一致
- [ ] CDC 命令不會在 HID 介面回應

## 📝 測試報告範例

成功測試的輸出應該類似：

```
==========================================================
ESP32-S3 整合測試工具
測試 CDC 和 HID 多通道回應功能
==========================================================

✅ pyserial 已安裝
✅ pywinusb 已安裝

=== 掃描 COM Ports (只掃描 USB CDC 裝置) ===
⏭️  跳過 COM1 (虛擬 COM port)
⏭️  跳過 COM5 (藍牙裝置)
嘗試 COM8... ✅ 找到！(HID_ESP32_S3)

=== 掃描 HID 介面 ===
✅ 找到 HID 裝置: TinyUSB HID

==========================================================
開始測試多通道回應
==========================================================

命令: *IDN?
📡 CDC 回應: HID_ESP32_S3
📡 HID 回應: HID_ESP32_S3
✅ 回應一致（多通道功能正常）

命令: INFO
📡 CDC 回應:
  裝置資訊:
  晶片型號: ESP32-S3
  自由記憶體: 295632 bytes
📡 HID 回應:
  裝置資訊:
  晶片型號: ESP32-S3
  自由記憶體: 295632 bytes
✅ 回應一致（多通道功能正常）

==========================================================
測試完成！
==========================================================

📊 測試總結:
  CDC 介面: ✅ 正常
  HID 介面: ✅ 正常
  多通道回應: ✅ 正常

✅ 所有介面正常運作
✅ 多通道回應功能已驗證
```

## 📌 注意事項

1. **Windows 驅動程式**：
   - Windows 10/11 通常會自動安裝 HID 驅動
   - 如果不行，可能需要 libusbK 或 WinUSB 驅動

2. **Linux 權限**：
   ```bash
   # 可能需要 udev 規則
   sudo usermod -a -G dialout $USER
   sudo usermod -a -G plugdev $USER
   ```

3. **macOS**：
   - 可能需要授予終端機存取權限
   - 系統偏好設定 → 安全性與隱私

4. **序列埠佔用**：
   - 關閉其他序列終端機（Arduino IDE, PuTTY 等）
   - 一次只能有一個程式開啟 COM port

5. **HID 裝置佔用**：
   - 關閉其他 HID 測試工具
   - Windows 可能需要重新插拔 USB 線

## 📚 相關文件

- **[README.md](README.md)** - 專案概述和快速開始
- **[PROTOCOL.md](PROTOCOL.md)** - HID 協定規格詳細說明
- **[CLAUDE.md](CLAUDE.md)** - AI 輔助開發專案說明

---

**測試工具**：Python + pyserial + pywinusb
**支援平台**：Windows
**測試覆蓋**：CDC 介面、HID 介面、多通道回應、協定切換
