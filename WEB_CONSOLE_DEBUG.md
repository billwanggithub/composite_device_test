# Web Console 調試指南

如果 Web Console 無法接收命令響應，請按照本指南進行排查。

## 快速檢查清單

### ✅ 第一步：驗證基本連接

1. **訪問 console.html**:
   ```
   http://192.168.4.1/console.html (AP Mode)
   或
   http://<device-ip>/console.html (Station Mode)
   ```

2. **檢查連接狀態**:
   - 頁面頂部應顯示 🟢 **已連接** (綠色指示器)
   - 如果顯示 🔴 **未連接**，檢查:
     - WiFi 連接是否正常
     - 設備是否已啟動
     - 瀏覽器防火牆設定

### ✅ 第二步：驗證 CDC 是否工作

通過 USB 連接設備，使用 CDC 序列埠發送命令：

```
> HELP
> INFO
> STATUS
```

**預期結果**：應看到完整的命令響應

**如果 CDC 也無法工作**：
- 問題可能在後端 CommandParser
- 檢查 CDC 是否正常工作（通過 PuTTY/Serial Monitor）

### ✅ 第三步：檢查瀏覽器控制台

1. 打開瀏覽器開發者工具：`F12`
2. 點擊 **Console** 標籤
3. 發送命令並檢查：
   - 是否有 JavaScript 錯誤？
   - WebSocket 連接是否正常？

**常見錯誤**:
```javascript
// 錯誤 1: WebSocket 連接失敗
WebSocket connection failed

// 錯誤 2: 跨域 CORS 問題
Cross-Origin Request Blocked

// 錯誤 3: 超時
WebSocket connection timeout
```

### ✅ 第四步：檢查 CDC 日誌

連接 USB 並打開 CDC 監視器，您應該看到：

```
[WS] 文本命令: HELP
[WS] 命令已處理: 是, 響應長度: 1234
[WS] 發送響應到客戶端 1: 1234 字節
```

**如果沒有看到這些日誌**：
- WebSocket 消息可能沒有到達後端
- 檢查 WiFi 連接

## 詳細排查步驟

### 場景 1: Web Console 連接成功但無響應

**症狀**：
- 頁面顯示 🟢 已連接
- 輸入命令並按 Enter
- 命令出現在控制台中
- 但沒有響應

**排查步驟**:

1. **檢查瀏覽器控制台 (F12)**:
   - 是否有任何 JavaScript 錯誤？
   - WebSocket 是否已連接？ (檢查 Network 標籤)

2. **檢查 CDC 日誌**:
   ```bash
   # 通過 CDC 監視器查看
   應該看到: [WS] 文本命令: 你的命令
   ```

3. **驗證命令格式**:
   - 確保命令是大寫 (如 `HELP` 不是 `help`)
   - 確保沒有額外的空格

4. **檢查 WebServer 日誌**:
   - 搜索 `[WS]` 日誌
   - 檢查是否有錯誤消息

### 場景 2: Web Console 無法連接

**症狀**：
- 頁面顯示 🔴 未連接
- 狀態欄顯示自動重連倒計時

**排查步驟**:

1. **驗證 WiFi 連接**:
   ```
   通過 CDC: > IP
   應該看到有效的 IP 地址
   ```

2. **驗證設備可訪問**:
   ```bash
   # 在你的電腦上 (Windows PowerShell)
   Test-Connection 192.168.4.1

   # 或 Linux/Mac
   ping 192.168.4.1
   ```

3. **檢查防火牆**:
   - Windows 防火牆是否阻止了連接？
   - 路由器防火牆設定如何？

4. **檢查瀏覽器控制台**:
   - 打開 F12 → Console
   - 查看是否有 WebSocket 連接錯誤

### 場景 3: 某些命令有響應，某些沒有

**症狀**：
- `INFO` 命令有響應
- `SET PWM` 命令無響應

**可能原因**:
1. 某些命令的實現有問題
2. 命令格式不正確
3. 命令參數缺失

**排查步驟**:

1. **通過 CDC 驗證命令**:
   - 在 CDC 上執行相同的命令
   - 如果 CDC 也無響應，問題在 CommandParser
   - 如果 CDC 有響應但 Web Console 沒有，問題在 WebSocket 路由

2. **檢查命令格式**:
   ```bash
   > HELP          # ✓ 正確
   > INFO          # ✓ 正確
   > STATUS        # ✓ 正確
   > SET PWM 20000 50   # ✓ 正確
   > set pwm 20000 50   # ✗ 錯誤 (應該大寫)
   ```

3. **查看完整的命令列表**:
   ```bash
   > HELP
   ```

## 啟用詳細日誌

如果上述步驟無法解決問題，可以啟用詳細的調試日誌：

### 方法 1: CDC 日誌（已啟用）

連接 USB 並打開 CDC 監視器 (115200 baud)，您將看到：

```
[WS] 文本命令: <你的命令>
[WS] 命令已處理: <是/否>
[WS] 響應長度: <字節數>
[WS] 發送響應到客戶端 <ID>: <字節數>
```

### 方法 2: 瀏覽器開發者工具

1. 打開 F12
2. 點擊 **Network** 標籤
3. 過濾 WebSocket：搜索 `ws:`
4. 發送命令
5. 在 WebSocket 連接上點擊
6. 檢查 **Messages** 標籤：
   - 發送的命令
   - 接收的響應

### 方法 3: 添加客戶端日誌

編輯 `console.html` 的 WebSocket 消息處理部分：

```javascript
ws.addEventListener('message', function(event) {
    console.log('[WS MSG]', event.data);  // 添加這行

    const message = event.data.trim();
    // ... 其餘代碼
});
```

然後在 F12 → Console 中查看所有接收的消息。

## 常見問題和解決方案

### Q: Web Console 出現亂碼或特殊字符

**A**: 檢查編碼設定。console.html 使用 UTF-8 編碼，確保：
- 頁面編碼設定為 UTF-8 (通常自動)
- 瀏覽器設定為 UTF-8

### Q: 命令出現在控制台但沒有響應，而且沒有錯誤消息

**A**: 可能是：
1. 後端沒有收到命令 - 檢查 CDC 日誌
2. 後端收到但沒有發送響應 - 檢查 CommandParser 實現
3. 響應被過濾掉了 - 檢查過濾邏輯

嘗試在 CDC 上執行相同的命令以確認。

### Q: 多個客戶端連接時，某些客戶端無法收到響應

**A**: 可能是：
1. 客戶端 ID 錯誤
2. AsyncWebSocket 連接限制 (最多 8 個)
3. 某個客戶端佔用了資源

檢查 CDC 日誌中的 `[WS] 發送響應到客戶端` 信息。

## 提交問題報告

如果問題仍未解決，請收集以下信息並提交報告：

1. **設備信息**:
   ```
   INFO 命令的輸出
   ```

2. **CDC 日誌** (使用 `HELP` 命令):
   - 完整的 CDC 輸出

3. **Web Console 消息**:
   - 你發送的命令
   - Web Console 的響應（如果有的話）

4. **瀏覽器信息**:
   - 瀏覽器類型和版本
   - 操作系統

5. **F12 控制台內容**:
   - 任何 JavaScript 錯誤

6. **WiFi 連接**:
   - SSID
   - 連接方式 (AP Mode / Station Mode)
   - IP 地址

## 快速修復檢查清單

- [ ] 設備已啟動且 WiFi 已連接
- [ ] URL 正確 (console.html)
- [ ] 頁面显示 🟢 已連接
- [ ] 通過 CDC 驗證命令有效
- [ ] 瀏覽器 F12 控制台無錯誤
- [ ] 命令使用大寫字母
- [ ] 命令格式正確
- [ ] 沒有額外的空格或字符
- [ ] 防火牆允許 WebSocket 連接
- [ ] 最新版本的固件已上傳

## 進階調試

### 啟用 AsyncWebServer 詳細日誌

編輯 `platformio.ini`，添加：

```ini
build_flags =
    ...
    -DCORE_DEBUG_LEVEL=5  # 最詳細的日誌
```

然後重新編譯並上傳。

### 查看 WebSocket 框架

在 `console.html` 中，添加日誌到 WebSocket 事件處理器：

```javascript
ws.addEventListener('open', function(event) {
    console.log('[WS OPEN]', event);
});

ws.addEventListener('close', function(event) {
    console.log('[WS CLOSE]', event);
});

ws.addEventListener('error', function(event) {
    console.log('[WS ERROR]', event);
});
```

---

## 总结

| 步驟 | 检查項 | 預期結果 |
|------|--------|--------|
| 1 | Web Console 顯示 🟢 連接 | 綠色連接指示器 |
| 2 | 通過 CDC 發送命令 | CDC 有響應 |
| 3 | 通過 Web Console 發送命令 | Web Console 有響應 |
| 4 | F12 控制台無錯誤 | 無 JavaScript 錯誤 |
| 5 | CDC 日誌顯示 [WS] 信息 | 見到相關日誌 |

如果完成這些步驟後仍有問題，請使用上述信息提交詳細的問題報告。

---

**最後更新**: 2025-11-10
**版本**: 1.0
