# Web Console 使用指南

## 概述

Web Console 是一個基於 WebSocket 的網頁控制台，提供與 CDC 序列埠相同的命令介面。您可以通過網頁瀏覽器發送命令，實時查看設備響應。

**版本**: v1.0
**創建日期**: 2025-11-10
**語言**: Traditional Chinese + English

## 功能特性

- ✅ **雙向通訊**: 使用 WebSocket 實現實時雙向通訊
- ✅ **完整命令支持**: 支持所有 CDC 命令（馬達控制、WiFi、周邊設備等）
- ✅ **命令歷史**: 使用 ↑↓ 箭頭鍵查看命令歷史
- ✅ **實時狀態**: 即時顯示設備連接狀態和消息計數
- ✅ **回車鍵自動執行**: 按 Enter 自動發送命令
- ✅ **專業終端風格**: 類似 Linux 終端的黑色主題界面
- ✅ **響應式設計**: 適配各種屏幕尺寸
- ✅ **自動重連**: WebSocket 斷開時自動重新連接
- ✅ **命令提示**: 內置幫助系統顯示常用命令

## 訪問方式

### 通過網頁瀏覽器

1. **確保設備連接到 WiFi**（AP Mode 或 Station Mode）
2. 在瀏覽器中輸入設備地址：
   - **AP Mode**: `http://192.168.4.1/console.html`
   - **Station Mode**: 使用 `IP` 命令查詢 IP 地址，然後訪問 `http://<your-device-ip>/console.html`

3. 頁面加載後，您應該看到：
   - 綠色的 "已連接" 狀態指示器
   - 歡迎訊息

### 導航欄

Web Console 提供快速導航菜單：
- 📊 **儀表板** - 馬達控制儀表板
- ⚙️ **設定** - WiFi 和系統設定
- 🔌 **周邊設備** - 周邊設備配置
- 🖥️ **控制台** - Web Console（當前頁面）

## 使用方法

### 基本操作

1. **輸入命令**: 在命令行中輸入命令
2. **發送命令**: 按 `Enter` 鍵發送
3. **查看響應**: 實時顯示在控制台中
4. **查看歷史**: 按 `↑` 鍵查看上一個命令，按 `↓` 鍵查看下一個命令

### 命令示例

```
> HELP
> INFO
> STATUS
> SET PWM 20000 50
> RPM
> MOTOR STATUS
> WIFI MyNetwork MyPassword
> IP
```

## 支持的命令

Web Console 支持與 CDC 序列埠相同的所有命令，包括：

### 基本命令

| 命令 | 描述 |
|------|------|
| `HELP` 或 `?` | 顯示幫助信息 |
| `INFO` | 顯示設備資訊 |
| `STATUS` | 顯示系統狀態 |
| `*IDN?` | SCPI 設備識別 |

### 馬達控制命令

| 命令 | 描述 |
|------|------|
| `SET PWM <freq> <duty>` | 設定 PWM 頻率 (Hz) 和佔空比 (%) |
| `SET PWM_DUTY <duty>` | 只設定佔空比 (%) |
| `SET POLE_PAIRS <num>` | 設定馬達極數 (1-12) |
| `SET MAX_FREQ <Hz>` | 設定最大頻率限制 |
| `RPM` | 讀取當前 RPM |
| `MOTOR STATUS` | 顯示馬達詳細狀態 |
| `MOTOR STOP` | 停止馬達（設定佔空比為 0%） |

### WiFi 命令

| 命令 | 描述 |
|------|------|
| `WIFI <ssid> <password>` | 連接到指定 WiFi 網絡 |
| `WIFI CONNECT` | 連接到已保存的 WiFi 網絡 |
| `IP` | 顯示 IP 地址和網絡資訊 |
| `AP ON` | 啟用接入點模式 |
| `AP OFF` | 禁用接入點模式 |
| `AP STATUS` | 顯示接入點狀態 |

### 周邊設備命令

| 命令 | 描述 |
|------|------|
| `UART1 MODE <UART\|PWM\|OFF>` | 切換 UART1 模式 |
| `UART1 PWM <freq> <duty>` | 設定 UART1 PWM |
| `UART2 CONFIG <baud>` | 配置 UART2 波特率 |
| `BUZZER <freq> <duty>` | 控制蜂鳴器 |
| `LED_PWM <freq> <brightness>` | 控制 LED |
| `RELAY <ON\|OFF>` | 控制繼電器 |
| `GPIO <HIGH\|LOW>` | 控制 GPIO |

### 設定命令

| 命令 | 描述 |
|------|------|
| `SAVE` | 保存所有設定到 NVS |
| `LOAD` | 從 NVS 加載設定 |
| `RESET` | 重置為出廠設定 |
| `PERIPHERAL SAVE` | 保存周邊設備設定 |
| `PERIPHERAL LOAD` | 加載周邊設備設定 |

## 技術細節

### WebSocket 連接

- **端點**: `/ws`
- **協議**: WebSocket (WS/WSS)
- **數據格式**: 純文本命令（與 CDC 相同）
- **自動重連**: 斷開後每 3 秒重試一次

### 命令處理

Web Console 使用統一的 `CommandParser` 系統處理命令，與 CDC、HID 和 BLE 使用相同的命令解析邏輯：

```
[Web 瀏覽器] ← WebSocket → [ESP32 WebServer] → [CommandParser] → [執行命令]
                                                      ↓
                                              [WebSocketResponse] ← 響應
```

### 響應機制

1. **即時響應**: 大多數命令在發送後立即返回響應
2. **多行響應**: 長的響應會自動分行顯示
3. **錯誤消息**: 無效命令會顯示 "❌ 未知命令" 錯誤
4. **狀態廣播**: 某些命令會觸發狀態更新廣播

## 故障排除

### 無法連接到 Web Console

**症狀**: 無法訪問 console.html，或頁面加載後顯示 "未連接"

**解決方案**:
1. 確保設備已啟動並連接到 WiFi
2. 驗證設備 IP 地址 (使用 CDC 的 `IP` 命令)
3. 確認 AP Mode 已啟用（如果在 AP Mode 中訪問）
4. 檢查防火牆設定，確保允許 WebSocket 連接

### 命令無法執行

**症狀**: 發送命令後沒有響應

**解決方案**:
1. 檢查網絡連接狀態指示器
2. 嘗試刷新頁面（F5）
3. 檢查瀏覽器控制台是否有錯誤信息
4. 通過 CDC 序列埠發送相同命令以驗證命令有效性

### 命令歷史不工作

**症狀**: 按 ↑ 鍵無法顯示之前的命令

**解決方案**:
1. 確保在命令輸入框中按鍵
2. 清除瀏覽器緩存
3. 重新加載頁面

## 與其他介面的比較

| 特性 | Web Console | CDC 序列埠 | HID | BLE |
|------|-----------|----------|-----|-----|
| 命令支持 | ✅ 完全 | ✅ 完全 | ✅ 完全 | ✅ 完全 |
| 實時性 | ✅ 優秀 | ✅ 優秀 | ✅ 優秀 | ✅ 優秀 |
| 易用性 | ✅ 最好 | ✅ 好 | ⚠️ 需工具 | ⚠️ 需客戶端 |
| WiFi 要求 | ✅ 是 | ❌ 否 | ❌ 否 | ⚠️ 可選 |
| 無線 | ✅ 是 | ❌ 否 | ❌ 否 | ✅ 是 |
| 多用戶 | ✅ 支持 | ❌ 一個 | ❌ 一個 | ⚠️ 受限 |

## 開發細節

### 後端實現

- **ResponseClass**: `WebSocketResponse` (CommandParser.h/cpp)
- **Server**: `AsyncWebServer` + `AsyncWebSocket` (WebServer.cpp)
- **消息處理**: `handleWebSocketMessage()` (WebServer.cpp:165)
- **命令解析**: `CommandParser::processCommand()` (CommandParser.cpp)

### 前端實現

- **文件**: `/data/console.html`
- **框架**: 純 JavaScript（無依賴）
- **通訊**: WebSocket API
- **存儲**: 瀏覽器本地（命令歷史）

## 限制和已知問題

1. **緩衝區限制**: 單個響應最大 ~1000 字符
2. **連接限制**: 同時最多支持 8 個 WebSocket 連接（可在 AsyncWebSocket 中配置）
3. **超時**: 沒有命令超時機制（命令將一直等待）
4. **長響應**: 非常長的響應（如大量 HELP 輸出）可能分多個包發送

## 未來改進

- [ ] 命令自動完成
- [ ] 語法高亮
- [ ] 實時日誌記錄
- [ ] 命令宏/腳本支持
- [ ] 圖形化監控面板
- [ ] ANSI 顏色碼支持
- [ ] 文件傳輸支持

## 相關文檔

- [README.md](README.md) - 項目概述
- [CLAUDE.md](CLAUDE.md) - AI 開發指南和命令參考
- [PROTOCOL.md](PROTOCOL.md) - 詳細協議規範

## 許可證和版本信息

**版本**: 1.0
**發佈日期**: 2025-11-10
**作者**: AI Assistant
**狀態**: Production Ready ✅

---

🖥️ **享受 Web Console！如有任何問題，請參閱故障排除部分。**
