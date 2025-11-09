# WebSocket 實現分析 - WebServer.cpp

## 簡要總結

WebSocket 已經在 WebServer.cpp 中**完全實現並運行中**。

---

## 1. WebSocket 實現狀態

### 已實現功能
✅ **WebSocket 對象已創建**
- 位置：`WebServer.h` L92
- 類型：`AsyncWebSocket* ws = nullptr;`
- 初始化：`begin()` 方法中創建 (WebServer.cpp L29)

✅ **WebSocket 端點**
- 路徑：`/ws`
- 協議：JSON 文本消息

✅ **WebSocket 生命週期管理**
- 連接事件：`WS_EVT_CONNECT` (L141)
- 斷開事件：`WS_EVT_DISCONNECT` (L148)
- 數據事件：`WS_EVT_DATA` (L152)
- 清理：`ws->cleanupClients()` (L72)

---

## 2. 現有的 WebSocket 處理邏輯

### 事件處理流程
```
setupWebSocket() [L127]
    ↓
handleWebSocketEvent() [L134]
    ├─ WS_EVT_CONNECT: 客戶端連接 → broadcastStatus()
    ├─ WS_EVT_DISCONNECT: 客戶端斷開
    └─ WS_EVT_DATA: 解析消息 → handleWebSocketMessage()
```

### 消息處理 (handleWebSocketMessage)
**位置**：WebServer.cpp L162-215

**支持的命令**：
| 命令 | 功能 | 實現方式 |
|------|------|--------|
| `set_freq` | 設置 PWM 頻率 | 通過 UART1Mux 設置 |
| `set_duty` | 設置佔空比 | 通過 UART1Mux 設置 |
| `stop` | 停止馬達 | 設置佔空比為 0% |
| `clear_error` | 清除錯誤 | 空操作 (v3.0 移除) |
| `get_status` | 獲取狀態 | 調用 broadcastStatus() |

**處理方式**：
- JSON 解析：`StaticJsonDocument<256>` (L172)
- 解析錯誤檢查：`if (error)` (L175)
- 命令提取：`const char* cmd = doc["cmd"]` (L180)
- 參數提取：`doc["value"]` (L187, L194)

---

## 3. 集成到命令解析系統

### 架構設計

**目前 WebSocket 與命令解析的分離**：

```
WebSocket 消息 (JSON)
    ↓
handleWebSocketMessage() [L162]
    ↓
命令識別 (strcmp)
    ↓
直接調用 UART1Mux 方法
    ├─ pPeripheralManager->getUART1().setPWMFrequency()
    ├─ pPeripheralManager->getUART1().setPWMDuty()
    └─ pPeripheralManager->getUART1().setPolePairs()
    ↓
broadcastStatus() 更新客戶端
```

### 與現有命令系統的對比

**CDC/HID 命令系統**（使用 CommandParser）：
- 通過文本命令解析器
- 支持響應路由（CDC、HID、BLE）
- 統一的命令格式

**WebSocket 命令系統**（當前）：
- 直接 JSON 命令解析
- 僅支持馬達控制命令
- 不使用 CommandParser

### 設計間隙
❌ WebSocket 不依賴 CommandParser
❌ 無法使用 CommandParser 的統一命令系統
❌ 無法支持 CommandParser 中的其他命令（WiFi、UART、GPIO 等）

---

## 4. 當前設計模式和架構

### 四層架構

```
[Web 前端 (HTML/JS)] ← WebSocket 連接 → [WebServer.cpp]
                                            ↓
                                      JSON 消息解析
                                            ↓
                              [handleWebSocketMessage()]
                                            ↓
                              [UART1Mux / PeripheralManager]
```

### 特點

1. **異步設計**
   - 使用 AsyncWebServer 和 AsyncWebSocket
   - 非阻塞操作
   - 支持多個並發客戶端

2. **廣播機制**
   - `broadcastStatus()` - 所有客戶端共享狀態 (L107)
   - `broadcastRPM()` - 廣播 RPM 數據 (L93)
   - 定期更新：每 200ms (WS_BROADCAST_INTERVAL_MS)

3. **客戶端管理**
   - 自動清理斷開的連接 (L72)
   - 追蹤客戶端數量 (L89-91)

### 前端實現

**JavaScript WebSocket 客戶端**（HTML 嵌入，L1219-1349）：

```javascript
連接邏輯：
- connectWebSocket(): 建立 ws:// 連接
- 自動重連：3 秒重試
- 連接狀態指示燈

消息處理：
- handleMessage(): 解析服務器消息
- 更新 UI 顯示 (RPM、頻率、佔空比等)

發送命令：
- setFrequency(): {cmd: 'set_freq', value: ...}
- setDuty(): {cmd: 'set_duty', value: ...}
- emergencyStop(): {cmd: 'stop'}
- clearError(): {cmd: 'clear_error'}
- getStatus(): {cmd: 'get_status'}
```

---

## 5. 現有命令與響應機制

### 廣播更新 (服務器 → 客戶端)

**位置**：`update()` 方法 (L66-83)

```cpp
定期檢查 (200ms):
├─ 清理斷開的客戶端
└─ 如果有客戶端連接:
   └─ broadcastStatus() // JSON 格式

broadcastStatus() 發送字段：
{
  "type": "status",
  "rpm": <float>,
  "raw_freq": <float>,
  "freq": <uint32_t>,
  "duty": <float>,
  "uptime": <seconds>
}
```

### REST API 與 WebSocket 並行

**REST API 端點** (setupRoutes, L217-423)：
- `/api/status` - 獲取狀態
- `/api/pwm` - 設置 PWM
- `/api/motor/freq` - 設置頻率
- `/api/motor/duty` - 設置佔空比
- `/api/uart1/status` - UART1 狀態
- 等 20+ 個端點

**WebSocket 命令**：
- 更精簡，只有 5 個命令
- 自動更新（廣播機制）
- 實時雙向通信

---

## 6. 集成到 CommandParser 的方案

### 當前限制
WebSocket 無法使用 CommandParser 系統，因為：
1. JSON 格式 vs 文本格式
2. 直接命令匹配 vs 統一解析器
3. 無狀態設計

### 可能的集成方式

**選項 A：WebSocket → CommandParser（推薦）**
```cpp
handleWebSocketMessage():
  1. 從 JSON 提取 cmd 和參數
  2. 構建文本命令格式
  3. 傳遞給 CommandParser
  4. 使用 WebSocketResponse 類處理響應
```

**選項 B：增強 WebSocket 支持**
```cpp
新增 WebSocketResponse 類:
- 實現 ICommandResponse 接口
- 直接發送 JSON 響應到客戶端
- 集成所有 CommandParser 命令
```

**選項 C：雙層系統（當前設計）**
- 保持 WebSocket 獨立
- 保持 CommandParser 獨立
- REST API 作為橋接

---

## 7. 總結表格

| 方面 | 狀態 | 詳情 |
|------|------|------|
| **WebSocket 實現** | ✅ 完整 | AsyncWebSocket，JSON 協議 |
| **WebSocket 命令** | ✅ 5 個命令 | set_freq, set_duty, stop, 等 |
| **廣播機制** | ✅ 完整 | 200ms 周期更新 |
| **與 CommandParser 集成** | ❌ 無 | 獨立實現 |
| **前端實現** | ✅ 完整 | 嵌入 HTML/JS，自動重連 |
| **REST API** | ✅ 完整 | 20+ 端點，覆蓋所有功能 |
| **客戶端管理** | ✅ 完整 | 自動清理，連接追蹤 |
| **錯誤處理** | ✅ 基本 | JSON 解析驗證 |

---

## 關鍵代碼位置

| 功能 | 文件 | 行號 |
|------|------|------|
| 初始化 | WebServer.cpp | L28-32 |
| 啟動 | WebServer.cpp | L35-56 |
| 更新循環 | WebServer.cpp | L66-83 |
| 事件處理 | WebServer.cpp | L127-160 |
| 消息解析 | WebServer.cpp | L162-215 |
| 廣播狀態 | WebServer.cpp | L107-125 |
| HTML/JS | WebServer.cpp | L1219-1349 |

---

## 建議

1. **如需統一命令系統**：創建 WebSocketResponse 類，集成 CommandParser
2. **如需更多命令**：擴展 `handleWebSocketMessage()` 中的命令列表
3. **如需持久化**：WebSocket 已正確調用 PeripheralManager 的 save/load 方法
