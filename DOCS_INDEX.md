# 📚 文件索引 / Documentation Index

本專案採用雙語文件策略，提供完整的繁體中文和英文文件。

This project uses a bilingual documentation strategy with comprehensive Traditional Chinese and English documentation.

## 🎯 快速導航 / Quick Navigation

### 我需要... / I need to...

| 需求 / Need | 繁體中文文件 / Chinese Doc | 英文文件 / English Doc |
|------------|------------------------|-------------------|
| **快速開始** / Quick Start | [README.md](README.md) | [README.md](README.md) |
| **了解命令** / Learn Commands | [README.md](README.md) § 可用命令 | [CLAUDE.md](CLAUDE.md) § Available Commands |
| **測試裝置** / Test Device | [TESTING.md](TESTING.md) | [TESTING.md](TESTING.md) |
| **了解協定** / Understand Protocol | [PROTOCOL.md](PROTOCOL.md) | [PROTOCOL.md](PROTOCOL.md) (部分英文) |
| **AI 開發** / AI Development | - | [CLAUDE.md](CLAUDE.md) |
| **實作細節** / Implementation Details | - | [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md) |
| **故障排除** / Troubleshooting | [README.md](README.md) § 故障排除 | [CLAUDE.md](CLAUDE.md) § Troubleshooting |

---

## 📖 主要文件 / Primary Documentation

### 1. README.md 📘
**語言 / Language**: 繁體中文 (Traditional Chinese)
**受眾 / Audience**: 終端使用者、開發者 (End users, Developers)

**內容 / Contents**:
- ✅ 專案概述和主要特性
- ✅ 硬體需求和軟體需求
- ✅ 快速開始指南（編譯、上傳、測試）
- ✅ **完整命令列表**（基本、馬達、WiFi、週邊、設定）
- ✅ Web 介面存取說明
- ✅ 硬體接腳定義和連接建議
- ✅ 專案結構
- ✅ 故障排除
- ✅ 開發板型號更換指南

**何時閱讀 / When to Read**:
- 首次使用本專案
- 需要查詢命令語法
- 硬體連接指導
- 基本故障排除

---

### 2. CLAUDE.md 📗
**語言 / Language**: English
**受眾 / Audience**: AI 助手、國際開發者 (AI assistants, International developers)

**內容 / Contents**:
- ✅ Project overview and architecture
- ✅ **Complete command reference** (organized by category)
- ✅ Critical configuration details
- ✅ Build commands and workflows
- ✅ Code structure and implementation decisions
- ✅ FreeRTOS architecture and task management
- ✅ USB/HID/BLE implementation details
- ✅ Board variant configuration guide
- ✅ Advanced troubleshooting

**何時閱讀 / When to Read**:
- AI-assisted development
- Understanding implementation details
- Modifying code or adding features
- Configuring different ESP32-S3 board variants
- English-speaking developers

---

### 3. TESTING.md 📙
**語言 / Language**: 繁體中文 (Traditional Chinese)
**受眾 / Audience**: 測試人員、QA、開發者 (Testers, QA, Developers)

**內容 / Contents**:
- ✅ 測試腳本使用說明 (test_hid.py, test_cdc.py, test_all.py, ble_client.py)
- ✅ **基本測試命令列表**
- ✅ **馬達和週邊控制命令範例**
- ✅ COM port 過濾策略
- ✅ 測試場景 (5 個詳細場景)
  - 場景 1: 快速驗證
  - 場景 2: HID 通訊調試
  - 場景 3: CDC 通訊調試
  - 場景 4: 回應路由驗證
  - **場景 5: 週邊控制測試** (新增)
- ✅ 回應路由驗證
- ✅ 進階測試技巧
- ✅ 故障排除

**何時閱讀 / When to Read**:
- 執行自動化測試
- 驗證裝置功能
- 調試通訊問題
- 測試週邊控制功能
- 驗證 DELAY 命令和腳本化控制

---

### 4. PROTOCOL.md 📕
**語言 / Language**: 繁體中文 (Traditional Chinese, 部分英文)
**受眾 / Audience**: 協定開發者、進階使用者 (Protocol developers, Advanced users)

**內容 / Contents**:
- ✅ 多介面命令協定規格
- ✅ HID 封包格式（0xA1 協定 vs 純文字協定）
- ✅ CDC 命令格式
- ✅ BLE GATT 命令格式
- ✅ 回應路由規則詳解
- ✅ FreeRTOS 任務架構
- ✅ 協定版本歷史

**何時閱讀 / When to Read**:
- 開發 HID 客戶端應用程式
- 了解協定格式細節
- 實作自訂命令解析
- 了解回應路由機制

---

## 🔧 實作和技術文件 / Implementation & Technical Docs

### 5. IMPLEMENTATION_GUIDE.md
**語言 / Language**: English
**受眾 / Audience**: 開發者 (Developers)

**內容 / Contents**:
- WiFi 和 Web Server 實作細節
- REST API 端點規格
- Captive Portal 實作
- 性能特性和記憶體使用

**何時閱讀 / When to Read**:
- 了解 WiFi 整合細節
- 開發 Web API 客戶端
- 優化效能

---

### 6. MOTOR_INTEGRATION_PLAN.md
**語言 / Language**: English
**受眾 / Audience**: 開發者 (Developers)

**內容 / Contents**:
- 馬達控制系統整合計畫
- MCPWM 配置
- RPM 測量實作

**何時閱讀 / When to Read**:
- 了解馬達控制實作
- 修改 PWM 或 RPM 功能

---

### 7. STATUS_LED_GUIDE.md
**語言 / Language**: English
**受眾 / Audience**: 開發者 (Developers)

**內容 / Contents**:
- WS2812 RGB LED 控制實作
- 狀態指示顏色定義
- RMT 週邊使用

**何時閱讀 / When to Read**:
- 自訂 LED 狀態指示
- 了解 RMT 實作

---

## 📝 專案管理文件 / Project Management Docs

以下文件為專案開發過程記錄，一般使用者不需閱讀：

### 開發記錄 / Development Records:
- `IMPLEMENTATION_MEMO.md` - 實作備忘錄
- `PROJECT_STATUS.md` - 專案狀態
- `WEB_IMPLEMENTATION_SUMMARY.md` - Web 實作摘要
- `WEB_SERVER_IMPROVEMENTS.md` - Web 伺服器改進
- `WIFI_WEBSERVER_INTEGRATION.md` - WiFi Web 伺服器整合
- `WEB_CLONE_PLAN.md` - Web 複製計畫

### 歷史記錄 / Historical Records:
- `CHANGELOG.md` - 變更日誌
- `BUILD_AND_TEST.md` - 建置和測試記錄
- `DOCUMENTATION_REVIEW.md` - 文件審查
- `PRIORITY3_TESTING.md` - 優先級 3 測試
- `REPOSITORY_REFERENCE_UPDATE.md` - 儲存庫參考更新

---

## 🌟 推薦閱讀順序 / Recommended Reading Order

### 新手使用者 / Beginners:
1. **README.md** - 了解專案和快速開始
2. **TESTING.md** - 執行測試驗證功能
3. **PROTOCOL.md** - (可選) 了解協定細節

### 開發者 / Developers:
1. **README.md** - 專案概述
2. **CLAUDE.md** - 完整技術細節
3. **PROTOCOL.md** - 協定規格
4. **IMPLEMENTATION_GUIDE.md** - 實作細節
5. **TESTING.md** - 測試策略

### AI 助手 / AI Assistants:
1. **CLAUDE.md** - 主要開發指南
2. **README.md** - 使用者視角
3. **PROTOCOL.md** - 協定細節

---

## 📊 文件完整度檢查表 / Documentation Completeness Checklist

### ✅ 已完成 / Completed:
- [x] 基本命令文件 (README.md, CLAUDE.md)
- [x] 馬達控制命令文件
- [x] WiFi 控制命令文件
- [x] **週邊控制命令文件** (UART1, UART2, Buzzer, LED, Relay, GPIO, Keys)
- [x] **DELAY 命令文件**
- [x] 硬體接腳定義（包含週邊）
- [x] 連接建議（包含週邊）
- [x] 測試場景（包含週邊測試）
- [x] 協定規格文件
- [x] 故障排除指南
- [x] 開發板型號配置指南

### 📝 文件特性 / Documentation Features:
- [x] 雙語策略 (繁體中文 + English)
- [x] 命令範例和預期輸出
- [x] 硬體連接圖
- [x] 測試腳本使用說明
- [x] 故障排除步驟
- [x] 交叉參考連結
- [x] 實用的測試場景
- [x] **週邊控制完整覆蓋**
- [x] **UART1 預設模式行為說明**

---

## 🔗 快速連結 / Quick Links

### 命令參考 / Command Reference:
- [完整命令列表 (中文)](README.md#可用命令)
- [Complete Command List (English)](CLAUDE.md#available-commands)
- [測試命令列表](TESTING.md#測試命令列表)

### 硬體 / Hardware:
- [硬體接腳定義 (中文)](README.md#硬體接腳定義)
- [硬體連接建議 (中文)](README.md#連接建議)

### 測試 / Testing:
- [測試場景](TESTING.md#測試場景)
- [週邊控制測試](TESTING.md#場景-5測試週邊控制功能)
- [回應路由驗證](TESTING.md#驗證回應路由功能)

### 開發 / Development:
- [新增命令](README.md#新增命令)
- [修改 HID 協定](README.md#修改-hid-協定)
- [Code Structure](CLAUDE.md#code-structure)
- [Build Commands](CLAUDE.md#build-commands)

---

## 📧 文件回饋 / Documentation Feedback

如果您發現文件有誤或需要改進，請：

If you find errors or need improvements in the documentation, please:

1. 提交 Issue 到 GitHub repository
2. 註明文件名稱和行號
3. 描述問題或建議的改進

---

**最後更新 / Last Updated**: 2025-11-08
**文件版本 / Documentation Version**: 1.2.0 (含週邊控制功能)
