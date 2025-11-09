# Web Console 快速開始指南

## 什麼是 Web Console？

Web Console 是一個基於 **WebSocket 雙向通訊** 的網頁版 CDC 控制台。您可以：

✅ 通過網頁瀏覽器發送命令
✅ 實時查看設備響應
✅ 支持所有 CDC 命令
✅ 命令歷史記錄 (↑↓ 箭頭鍵)

## 快速開始 (3 步)

### 1️⃣ 確保設備連接到 WiFi

使用 CDC 或 HID 執行命令：
```
WIFI YourSSID YourPassword
```

或使用 AP Mode（AP Mode 默認啟用）：
```
AP MODE: http://192.168.4.1
```

### 2️⃣ 訪問 Web Console

在瀏覽器中打開：
```
http://192.168.4.1/console.html        (AP Mode)
http://<your-device-ip>/console.html   (Station Mode)
```

### 3️⃣ 發送命令

```
> HELP          # 查看所有命令
> INFO          # 查看設備資訊
> STATUS        # 查看系統狀態
> SET PWM 20000 50     # 設定頻率和佔空比
> RPM           # 讀取 RPM
```

## 常用命令速查

| 功能 | 命令 |
|------|------|
| 查看幫助 | `HELP` |
| 設備資訊 | `INFO` |
| 系統狀態 | `STATUS` |
| **馬達** | |
| 設定 PWM | `SET PWM 20000 50` (freq, duty%) |
| 讀取 RPM | `RPM` |
| 停止馬達 | `MOTOR STOP` |
| **WiFi** | |
| 連接網絡 | `WIFI SSID Password` |
| 查看 IP | `IP` |
| **保存設定** | |
| 保存 | `SAVE` |
| 加載 | `LOAD` |
| 重置 | `RESET` |

## 實現細節

### 架構

```
┌─────────────┐
│  瀏覽器      │
│ console.html │
└──────┬──────┘
       │ WebSocket
       │ (雙向通訊)
       ▼
┌──────────────────┐
│   ESP32 WebServer │
│  (AsyncWebServer)│
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│  CommandParser   │
│ (統一命令系統)   │
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│  WebSocketResponse
│ (響應處理)       │
└──────────────────┘
```

### 新增文件

| 文件 | 用途 | 狀態 |
|------|------|------|
| `data/console.html` | Web Console 前端界面 | ✅ 新增 |
| `WEB_CONSOLE.md` | 完整使用文檔 | ✅ 新增 |
| `src/CommandParser.h` | WebSocketResponse 類定義 | ✅ 更新 |
| `src/CommandParser.cpp` | WebSocketResponse 實現 | ✅ 更新 |
| `src/WebServer.cpp` | WebSocket 命令集成 | ✅ 更新 |

## 與其他介面的比較

| 功能 | Web Console | CDC 序列埠 | HID | BLE |
|------|-----------|----------|-----|-----|
| 易用性 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ |
| 實時性 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| 無線 | ✅ | ❌ | ❌ | ✅ |
| 多用戶 | ✅ (多達 8 個) | ❌ | ❌ | ❌ |
| 命令支持 | ✅ 完全 | ✅ 完全 | ✅ 完全 | ✅ 完全 |

## 故障排除

### ❌ 無法連接

**檢查清單**:
- ✓ 設備已啟動？
- ✓ 已連接到 WiFi？
- ✓ IP 地址正確？
- ✓ 頁面已載入？

**解決方案**:
1. 刷新頁面 (F5)
2. 檢查設備 IP: `IP` (通過 CDC)
3. 檢查瀏覽器控制台: F12 → Console
4. 嘗試 AP Mode: http://192.168.4.1/console.html

### ❌ 命令無反應

**檢查清單**:
- ✓ 連接狀態指示器為綠色？
- ✓ 命令輸入框已聚焦？
- ✓ 按 Enter 鍵？

**解決方案**:
1. 等待 1-2 秒，有時設備需要時間處理
2. 通過 CDC 驗證命令有效性
3. 嘗試簡單命令: `INFO`
4. 重新加載頁面

### ❌ 連接斷開

**正常行為**: Web Console 會自動重新連接（每 3 秒重試）

**恢復方式**:
1. 等待自動重新連接（最多 3 秒）
2. 手動刷新頁面
3. 檢查設備連接狀態

## 下一步

👉 查看完整文檔: [WEB_CONSOLE.md](WEB_CONSOLE.md)

👉 查看命令參考: [CLAUDE.md](CLAUDE.md) 或通過 `HELP` 命令

👉 探索其他功能:
- 📊 [儀表板](/) - 馬達控制和監控
- ⚙️ [設定](/settings.html) - WiFi 和系統設定
- 🔌 [周邊設備](/peripherals.html) - 控制 UART、蜂鳴器、LED 等

---

## 技術支持

有問題？試試：

1. **查看日誌**: 通過 CDC 檢查設備輸出
2. **查看幫助**: 在 Web Console 中輸入 `HELP`
3. **閱讀文檔**: 查看 [WEB_CONSOLE.md](WEB_CONSOLE.md) 和 [CLAUDE.md](CLAUDE.md)

---

**版本**: 1.0
**狀態**: ✅ 生產就緒
**最後更新**: 2025-11-10
