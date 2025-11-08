# 馬達控制合併到 UART1 架構重構計劃

## 📋 變更需求

**目標：**
1. ✅ 取消原來的馬達控制 pins（GPIO 10, 11）
2. ✅ 將馬達控制功能完整合併到 UART1 的 PWM/RPM（GPIO 17, 18）
3. ✅ UART1 的 PWM/RPM 功能要和原來的馬達控制一樣

## 🔍 當前架構

### 原馬達控制（即將移除）

**硬體：**
- GPIO 10: Motor PWM 輸出（MCPWM_UNIT_1, Timer 0）
- GPIO 11: Tachometer 輸入（MCPWM_UNIT_0, CAP0）
- GPIO 12: Change notification pulse

**功能：**
- PWM 頻率控制：10 Hz - 500 kHz
- PWM 占空比控制：0 - 100%
- RPM 測量：MCPWM Capture（高精度）
- 極數設定：1-12
- 最大頻率限制
- 設定持久化（NVS）

**命令：**
- `SET PWM_FREQ <Hz>`
- `SET PWM_DUTY <%>`
- `SET POLE_PAIRS <num>`
- `RPM`
- `MOTOR STATUS`
- `MOTOR STOP`

### UART1 多工器（當前）

**硬體：**
- GPIO 17: UART1 TX / **LEDC PWM**（1 Hz - 500 kHz，13-bit 限制 ~9.7 kHz）
- GPIO 18: UART1 RX / **MCPWM Capture**（1 Hz - 500 kHz，已升級）

**功能：**
- 三種模式：UART / PWM_RPM / OFF
- UART 模式：串口通信
- PWM_RPM 模式：
  - PWM 輸出（LEDC，頻率受限）
  - RPM 測量（MCPWM Capture）

**命令：**
- `UART1 MODE <UART|PWM|OFF>`
- `UART1 PWM <freq> <duty> [ON|OFF]`
- `UART1 STATUS`

## 🎯 目標架構

### 新的 UART1 多工器（整合馬達控制）

**硬體：**
- GPIO 17: UART1 TX / **MCPWM PWM**（10 Hz - 500 kHz，無 LEDC 限制）
- GPIO 18: UART1 RX / **MCPWM Capture**（1 Hz - 500 kHz）

**功能：**
- 三種模式：UART / MOTOR_CONTROL / OFF
- UART 模式：保持不變
- MOTOR_CONTROL 模式（原 PWM_RPM）：
  - ✅ PWM 輸出（**MCPWM** 替代 LEDC，移除 9.7 kHz 限制）
  - ✅ RPM 測量（MCPWM Capture，已有）
  - ✅ 極數設定
  - ✅ 最大頻率限制
  - ✅ 設定持久化
  - ✅ 所有原馬達控制功能

**命令（向下兼容）：**
- 保留原 `UART1` 命令
- 保留原 `MOTOR` / `SET` 命令（內部路由到 UART1）

## 📊 關鍵技術變更

### 1. UART1 PWM：LEDC → MCPWM

**當前（LEDC）：**
```cpp
// LEDC 13-bit 分辨率
ledc_timer_config_t timer_conf = {
    .duty_resolution = LEDC_TIMER_13_BIT,  // 限制 max ~9.7 kHz
    .freq_hz = frequency,
};
```

**目標（MCPWM）：**
```cpp
// MCPWM 無分辨率限制
mcpwm_config_t pwm_config;
pwm_config.frequency = frequency;  // 10 Hz - 500 kHz
pwm_config.cmpr_a = duty;
mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_0, &pwm_config);
```

### 2. MCPWM 資源分配

**當前：**
- MCPWM_UNIT_0:
  - CAP0 (GPIO 11): 馬達 Tachometer
  - CAP1 (GPIO 18): UART1 RPM
- MCPWM_UNIT_1:
  - Timer 0 (GPIO 10): 馬達 PWM

**目標：**
- MCPWM_UNIT_0:
  - CAP0: **釋放**（GPIO 11 不再使用）
  - CAP1 (GPIO 18): UART1 RPM ✅
- MCPWM_UNIT_1:
  - Timer 0 (GPIO 17): **UART1 PWM** ✅（從 GPIO 10 移到 GPIO 17）

### 3. 功能整合

**從 MotorControl 移到 UART1Mux：**

| 功能 | 當前位置 | 目標位置 |
|------|---------|---------|
| PWM 頻率控制 | MotorControl | UART1Mux |
| PWM 占空比控制 | MotorControl | UART1Mux |
| RPM 測量 | MotorControl | UART1Mux（已有） |
| 極數設定 | MotorControl | UART1Mux（新增） |
| 最大頻率限制 | MotorControl | UART1Mux（新增） |
| 設定持久化 | MotorSettingsManager | UART1Mux（新增） |

## 📝 實作步驟

### 階段 1：修改 UART1Mux（LEDC → MCPWM）

**1.1 修改 PeripheralPins.h**
```cpp
// 更新 GPIO 17 註解
#define PIN_UART1_TX  17  // UART1 TX / MCPWM PWM (10Hz-500kHz)

// 新增 MCPWM 定義
#define MCPWM_UNIT_UART1_PWM    MCPWM_UNIT_1
#define MCPWM_TIMER_UART1_PWM   MCPWM_TIMER_0
#define MCPWM_GEN_UART1_PWM     MCPWM_OPR_A

// 移除 LEDC 定義（或標記為廢棄）
// #define LEDC_CHANNEL_UART1_PWM  2
// #define LEDC_TIMER_UART1_PWM    2
```

**1.2 修改 UART1Mux.h**
```cpp
// 移除 LEDC，新增 MCPWM 相關變數
#include "driver/mcpwm.h"

// 新增馬達控制參數
uint32_t polePairs = 2;           // 極數（預設 2）
uint32_t maxFrequency = 100000;   // 最大頻率限制
```

**1.3 修改 UART1Mux.cpp**
- `initPWM()`: 從 LEDC 改為 MCPWM
- `deinitPWM()`: 從 LEDC 改為 MCPWM
- `setPWMFrequency()`: 使用 `mcpwm_set_frequency()`
- `setPWMDuty()`: 使用 `mcpwm_set_duty()`
- `setPWMEnabled()`: 使用 `mcpwm_start()` / `mcpwm_stop()`

### 階段 2：整合馬達控制功能

**2.1 新增馬達控制方法到 UART1Mux**
```cpp
class UART1Mux {
public:
    // 馬達控制方法（從 MotorControl 移植）
    bool setPolePairs(uint32_t poles);
    uint32_t getPolePairs() const { return polePairs; }
    bool setMaxFrequency(uint32_t freq);
    uint32_t getMaxFrequency() const { return maxFrequency; }
    float getCalculatedRPM() const;  // 根據極數計算實際 RPM

    // 設定持久化
    bool saveSettings();
    bool loadSettings();
    void resetToDefaults();
};
```

**2.2 實作方法**
```cpp
float UART1Mux::getCalculatedRPM() const {
    // RPM = (頻率 × 60) / 極數
    return (rpmFrequency * 60.0) / (float)polePairs;
}
```

### 階段 3：更新命令處理

**3.1 保持向下兼容**
```cpp
// CommandParser.cpp

// 原 MOTOR 命令路由到 UART1
void CommandParser::handleSetPWMFreq(cmd, response) {
    peripheralManager.getUART1().setPWMFrequency(freq);
}

void CommandParser::handleSetPWMDuty(cmd, response) {
    peripheralManager.getUART1().setPWMDuty(duty);
}

void CommandParser::handleSetPolePairs(cmd, response) {
    peripheralManager.getUART1().setPolePairs(poles);
}

void CommandParser::handleRPM(response) {
    float rpm = peripheralManager.getUART1().getCalculatedRPM();
    response->printf("RPM: %.1f\n", rpm);
}
```

**3.2 UART1 命令保持不變**
- `UART1 MODE PWM` 等同於進入馬達控制模式
- `UART1 PWM <freq> <duty>` 控制馬達 PWM
- `UART1 STATUS` 顯示馬達狀態（包含極數、RPM）

### 階段 4：清理舊代碼

**4.1 移除文件**
- `src/MotorControl.h`
- `src/MotorControl.cpp`
- `src/MotorSettingsManager.h`
- `src/MotorSettingsManager.cpp`（部分功能整合到 UART1Mux）

**4.2 更新 PeripheralPins.h**
```cpp
// 標記為廢棄
// DEPRECATED: Motor control merged to UART1
// #define PIN_MOTOR_PWM_OUTPUT     10
// #define PIN_MOTOR_TACH_INPUT     11
// #define PIN_MOTOR_PULSE_OUTPUT   12
```

**4.3 更新 main.cpp**
- 移除 `MotorControl motorControl;`
- 移除 `MotorSettingsManager motorSettingsManager;`
- 所有馬達控制呼叫改為 `peripheralManager.getUART1()`

### 階段 5：更新 WebServer

**5.1 API 端點保持不變**
```cpp
// /api/pwm, /api/rpm 等端點內部改為呼叫 UART1
server.on("/api/pwm", HTTP_POST, []() {
    peripheralManager.getUART1().setPWMFrequency(freq);
    peripheralManager.getUART1().setPWMDuty(duty);
});
```

## 🎁 優勢

### 技術優勢

1. **✅ 移除 LEDC 頻率限制**
   - 舊：LEDC 13-bit 限制 ~9.7 kHz
   - 新：MCPWM 完整支援 10 Hz - 500 kHz

2. **✅ 統一 MCPWM 架構**
   - PWM 和 Capture 都使用 MCPWM
   - 更一致、更可靠

3. **✅ 節省 GPIO**
   - 釋放 GPIO 10, 11, 12
   - 可用於其他功能

4. **✅ 簡化架構**
   - 移除 MotorControl 類
   - 減少代碼複雜度

### 使用者優勢

1. **✅ 向下兼容**
   - 原 `MOTOR` 命令繼續工作
   - 原 Web API 繼續工作

2. **✅ 功能增強**
   - UART1 PWM 頻率範圍擴大
   - 高精度 RPM 測量

3. **✅ 統一介面**
   - 所有馬達控制通過 UART1
   - 更簡潔的概念模型

## ⚠️ 風險評估

### 高風險項目

1. **MCPWM 資源衝突**
   - ⚠️ MCPWM_UNIT_1 是否已被其他功能使用？
   - 解決：檢查整個專案的 MCPWM 使用情況

2. **GPIO 17 MCPWM 路由**
   - ⚠️ GPIO 17 是否支援 MCPWM？需要 GPIO Matrix 配置
   - 解決：使用 `mcpwm_gpio_init()` 正確配置

3. **向下兼容性**
   - ⚠️ 確保舊命令繼續工作
   - 解決：保留所有命令處理，內部路由到 UART1

### 中風險項目

1. **NVS 設定遷移**
   - 舊：MotorSettingsManager
   - 新：整合到 UART1Mux
   - 解決：保留相同的 NVS key 名稱

2. **WebServer 整合**
   - 需更新所有 API 呼叫
   - 解決：逐一檢查並測試

## 📅 工作量估算

| 階段 | 工作量 | 優先級 |
|------|--------|--------|
| 1. UART1 LEDC→MCPWM | 2-3 小時 | 最高 |
| 2. 整合馬達控制功能 | 3-4 小時 | 最高 |
| 3. 更新命令處理 | 1-2 小時 | 高 |
| 4. 清理舊代碼 | 1 小時 | 中 |
| 5. 更新 WebServer | 2 小時 | 高 |
| 6. 測試驗證 | 2-3 小時 | 最高 |

**總計：11-15 小時**

## ✅ 測試檢查清單

### 基本功能測試

- [ ] UART1 MODE UART - UART 模式正常工作
- [ ] UART1 MODE PWM - 切換到馬達控制模式
- [ ] UART1 PWM 100 50 ON - 低頻 PWM 輸出
- [ ] UART1 PWM 10000 50 ON - 高頻 PWM 輸出（應無 LEDC 限制）
- [ ] UART1 STATUS - 顯示完整馬達狀態

### 馬達控制測試

- [ ] SET PWM_FREQ 1000 - 設定頻率
- [ ] SET PWM_DUTY 75 - 設定占空比
- [ ] SET POLE_PAIRS 4 - 設定極數
- [ ] RPM - 顯示計算的 RPM
- [ ] MOTOR STATUS - 顯示詳細狀態
- [ ] MOTOR STOP - 緊急停止

### 持久化測試

- [ ] SAVE - 保存設定
- [ ] LOAD - 載入設定
- [ ] 重啟後設定保留

### WebServer 測試

- [ ] /api/status - 狀態查詢
- [ ] /api/pwm - PWM 控制
- [ ] /api/rpm - RPM 讀取
- [ ] Web 儀表板正常運作

## 🚦 決策建議

**我的建議：✅ 值得執行**

**理由：**
1. ✅ 移除 LEDC 限制，技術債清償
2. ✅ 架構更簡潔、更統一
3. ✅ 釋放 GPIO 資源
4. ✅ 向下兼容，使用者無感升級

**但需注意：**
- ⚠️ 工作量較大（11-15 小時）
- ⚠️ 需充分測試，確保穩定性
- ⚠️ 建議分階段實施，每階段都要測試

---

## 💬 確認問題

請回答以下問題，我們再開始實作：

1. **是否確定要執行此重構？**
   - [ ] 是，立即開始
   - [ ] 否，需要更多信息

2. **是否需要保留 GPIO 10, 11 的馬達控制作為備用？**
   - [ ] 完全移除（建議）
   - [ ] 保留作為備用選項

3. **命令介面偏好？**
   - [ ] 保持向下兼容（`MOTOR` 和 `UART1` 命令都保留）
   - [ ] 統一使用 `UART1` 命令
   - [ ] 統一使用 `MOTOR` 命令

4. **實施策略？**
   - [ ] 一次性完成所有變更
   - [ ] 分階段實施（推薦）

**請告訴我您的決定！** 🚀
