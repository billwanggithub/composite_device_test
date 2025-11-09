#include "UART1Mux.h"
#include "driver/gpio.h"
#include "soc/mcpwm_periph.h"
#include "soc/mcpwm_struct.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Preferences.h>
#include "USBCDC.h"

// External reference to USBSerial (defined in main.cpp)
extern USBCDC USBSerial;

// NVS namespace for UART1 settings persistence
static const char* NVS_NAMESPACE = "uart1_settings";

// Define static variables for MCPWM Capture
volatile uint32_t UART1Mux::capturePeriod = 0;
volatile bool UART1Mux::newCaptureAvailable = false;
volatile unsigned long UART1Mux::lastCaptureTime = 0;

UART1Mux::UART1Mux() {
    // Initialize GPIO 12 for PWM parameter change pulse (glitch observation)
    initPWMChangePulse();
}

UART1Mux::~UART1Mux() {
    disable();
}

// ============================================================================
// Mode Control
// ============================================================================

bool UART1Mux::setModeUART(uint32_t baudRate, uart_stop_bits_t stopBits,
                           uart_parity_t parity, uart_word_length_t dataBits) {
    // Validate parameters
    if (!validateUARTConfig(baudRate, stopBits, parity, dataBits)) {
        return false;
    }

    // If already in UART mode, just reconfigure
    if (currentMode == MODE_UART) {
        return reconfigureUART(baudRate, stopBits, parity, dataBits);
    }

    // Disable current mode
    disable();

    // Save UART configuration
    uartBaudRate = baudRate;
    uartStopBits = stopBits;
    uartParity = parity;
    uartDataBits = dataBits;

    // Initialize UART
    if (!initUART()) {
        USBSerial.println("[UART1] Failed to initialize UART mode");
        return false;
    }

    currentMode = MODE_UART;
    USBSerial.printf("[UART1] Switched to UART mode: %u baud\n", baudRate);

    // Settling time
    delay(10);

    return true;
}

bool UART1Mux::setModePWM_RPM() {
    // If already in PWM_RPM mode, nothing to do
    if (currentMode == MODE_PWM_RPM) {
        return true;
    }

    // Disable current mode
    disable();

    // Initialize PWM and RPM capture
    bool pwmOK = initPWM();
    bool rpmOK = initRPM();

    if (!pwmOK || !rpmOK) {
        USBSerial.println("[UART1] Failed to initialize PWM/RPM mode");
        disable();
        return false;
    }

    currentMode = MODE_PWM_RPM;
    USBSerial.println("[UART1] Switched to PWM/RPM mode");

    // Settling time
    delay(10);

    return true;
}

void UART1Mux::disable() {
    switch (currentMode) {
        case MODE_UART:
            deinitUART();
            break;
        case MODE_PWM_RPM:
            deinitPWM();
            deinitRPM();
            break;
        case MODE_DISABLED:
            // Already disabled
            break;
    }

    releasePins();
    currentMode = MODE_DISABLED;
}

const char* UART1Mux::getModeName() const {
    switch (currentMode) {
        case MODE_UART: return "UART";
        case MODE_PWM_RPM: return "PWM/RPM";
        case MODE_DISABLED: return "DISABLED";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// UART Mode Functions
// ============================================================================

int UART1Mux::write(const uint8_t* data, size_t len, uint32_t timeoutMs) {
    if (currentMode != MODE_UART || data == nullptr || len == 0) {
        return -1;
    }

    int written = uart_write_bytes(uartNum, (const char*)data, len);
    if (written > 0) {
        uartTxBytes += written;
    } else {
        uartErrors++;
    }

    if (timeoutMs > 0) {
        uart_wait_tx_done(uartNum, pdMS_TO_TICKS(timeoutMs));
    }

    return written;
}

int UART1Mux::write(const char* str, uint32_t timeoutMs) {
    if (str == nullptr) {
        return -1;
    }
    return write((const uint8_t*)str, strlen(str), timeoutMs);
}

int UART1Mux::read(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs) {
    if (currentMode != MODE_UART || buffer == nullptr || maxLen == 0) {
        return -1;
    }

    int len = uart_read_bytes(uartNum, buffer, maxLen, pdMS_TO_TICKS(timeoutMs));
    if (len > 0) {
        uartRxBytes += len;
    } else if (len < 0) {
        uartErrors++;
    }

    return len;
}

int UART1Mux::available() {
    if (currentMode != MODE_UART) {
        return 0;
    }

    size_t len = 0;
    uart_get_buffered_data_len(uartNum, &len);
    return (int)len;
}

void UART1Mux::clearRxBuffer() {
    if (currentMode != MODE_UART) {
        return;
    }
    uart_flush_input(uartNum);
}

bool UART1Mux::reconfigureUART(uint32_t baudRate, uart_stop_bits_t stopBits,
                               uart_parity_t parity, uart_word_length_t dataBits) {
    if (currentMode != MODE_UART) {
        return false;
    }

    if (!validateUARTConfig(baudRate, stopBits, parity, dataBits)) {
        return false;
    }

    // Flush TX before reconfiguration
    uart_wait_tx_done(uartNum, pdMS_TO_TICKS(1000));

    // Configure new parameters
    uart_config_t uart_config = {
        .baud_rate = (int)baudRate,
        .data_bits = dataBits,
        .parity = parity,
        .stop_bits = stopBits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t err = uart_param_config(uartNum, &uart_config);
    if (err != ESP_OK) {
        USBSerial.printf("[UART1] Reconfigure failed: %d\n", err);
        return false;
    }

    uartBaudRate = baudRate;
    uartStopBits = stopBits;
    uartParity = parity;
    uartDataBits = dataBits;

    USBSerial.printf("[UART1] Reconfigured: %u baud\n", baudRate);
    return true;
}

// ============================================================================
// PWM/RPM Mode Functions
// ============================================================================

bool UART1Mux::setPWMFrequency(uint32_t frequency) {
    if (currentMode != MODE_PWM_RPM) {
        return false;
    }

    if (!validatePWMFrequency(frequency)) {
        return false;
    }

    // Output pulse on GPIO 12 BEFORE changing frequency (to observe glitches)
    outputPWMChangePulse();

    // TRULY GLITCH-FREE UPDATE USING LL API
    // Calculate new prescaler and period for target frequency
    uint32_t new_prescaler, new_period;
    calculatePWMParameters(frequency, new_prescaler, new_period);

    // Check if prescaler needs to change
    if (new_prescaler != pwmPrescaler) {
        // Prescaler change required - must use high-level API (will stop PWM briefly)
        USBSerial.printf("[UART1] ⚠️ Prescaler change required (%u → %u), brief PWM stop unavoidable\n",
                     pwmPrescaler, new_prescaler);

        esp_err_t err = mcpwm_set_frequency(MCPWM_UNIT_UART1_PWM, MCPWM_TIMER_UART1_PWM, frequency);
        if (err != ESP_OK) {
            USBSerial.printf("[UART1] PWM frequency set failed: %s\n", esp_err_to_name(err));
            return false;
        }

        // Update stored values
        pwmPrescaler = new_prescaler;
        pwmPeriod = new_period;
        pwmFrequency = frequency;

        USBSerial.printf("[UART1] PWM frequency updated: %u Hz (prescaler=%u, period=%u)\n",
                     frequency, pwmPrescaler, pwmPeriod);
    } else {
        // Same prescaler - update period only using LL API (no PWM stop!)
        updatePWMRegistersDirectly(new_period, pwmDuty);

        // Update stored values
        pwmPeriod = new_period;
        pwmFrequency = frequency;

        USBSerial.printf("[UART1] PWM frequency updated (no-stop): %u Hz (period=%u)\n",
                     frequency, pwmPeriod);
    }

    return true;
}

bool UART1Mux::setPWMDuty(float duty) {
    if (currentMode != MODE_PWM_RPM) {
        return false;
    }

    if (duty < 0.0 || duty > 100.0) {
        return false;
    }

    // Output pulse on GPIO 12 BEFORE changing duty cycle (to observe glitches)
    outputPWMChangePulse();

    // TRULY GLITCH-FREE UPDATE USING LL API
    // Update duty shadow register only (period unchanged)
    updatePWMRegistersDirectly(pwmPeriod, duty);

    pwmDuty = duty;

    USBSerial.printf("[UART1] PWM duty updated (no-stop, LL API): %.1f%%\n", duty);

    return true;
}

bool UART1Mux::setPWMFrequencyAndDuty(uint32_t frequency, float duty) {
    // ==== ENTRY DEBUG ====
    USBSerial.printf("[UART1] 🚀 setPWMFrequencyAndDuty() ENTRY: freq=%u Hz, duty=%.1f%%\n", frequency, duty);
    USBSerial.printf("[UART1] 📊 Current: prescaler=%u, period=%u, freq=%u, duty=%.1f\n",
                     pwmPrescaler, pwmPeriod, pwmFrequency, pwmDuty);
    USBSerial.flush();

    if (currentMode != MODE_PWM_RPM) {
        USBSerial.println("[UART1] ❌ ABORT: Not in PWM_RPM mode");
        return false;
    }

    // Validate parameters
    if (!validatePWMFrequency(frequency)) {
        USBSerial.println("[UART1] ❌ ABORT: Frequency validation failed");
        return false;
    }

    if (duty < 0.0 || duty > 100.0) {
        USBSerial.println("[UART1] ❌ ABORT: Duty validation failed");
        return false;
    }

    // Mark PWM parameter change with GPIO12 toggle (non-blocking, glitch-free)
    outputPWMChangePulse();

    // SMART FREQUENCY UPDATE STRATEGY:
    // Try to achieve target frequency using CURRENT prescaler first
    // This maximizes use of shadow register mode (glitch-free)

    // Use detected clock frequency instead of hardcoded 80MHz
    uint32_t target_ticks = mcpwmClockFreq / frequency;

    // Try using current prescaler
    uint32_t new_period_with_current_prescaler = target_ticks / pwmPrescaler;

    USBSerial.printf("[UART1] 🧮 Clock=%u Hz, Target freq=%u Hz, current prescaler=%u\n",
                 mcpwmClockFreq, frequency, pwmPrescaler);
    USBSerial.printf("[UART1] 🧮 New period with current prescaler: %u ticks\n", new_period_with_current_prescaler);
    USBSerial.flush();

    // Check if new period is valid (2 to 65535)
    if (new_period_with_current_prescaler >= 2 && new_period_with_current_prescaler <= 65535) {
        // Can achieve target frequency with current prescaler!
        // Use shadow register mode (glitch-free!)
        USBSerial.printf("[UART1] ✅ GLITCH-FREE PATH: Keep prescaler=%u, period: %u → %u\n",
                     pwmPrescaler, pwmPeriod, new_period_with_current_prescaler);
        USBSerial.flush();

        USBSerial.println("[UART1] 🔧 Calling updatePWMRegistersDirectly()...");
        updatePWMRegistersDirectly(new_period_with_current_prescaler, duty);
        USBSerial.println("[UART1] ✅ updatePWMRegistersDirectly() returned");

        // Update stored values
        pwmPeriod = new_period_with_current_prescaler;
        pwmFrequency = frequency;
        pwmDuty = duty;

        USBSerial.printf("[UART1] ✅ PWM updated (glitch-free): %u Hz, %.1f%%\n", frequency, duty);
        USBSerial.flush();
    } else {
        // Cannot achieve target frequency with current prescaler
        // Must change prescaler - use mcpwm_set_frequency() (may glitch)
        USBSerial.printf("[UART1] ⚠️ PRESCALER CHANGE REQUIRED: period %u out of range [2, 65535]\n",
                     new_period_with_current_prescaler);
        USBSerial.flush();

        esp_err_t err_freq = mcpwm_set_frequency(MCPWM_UNIT_UART1_PWM, MCPWM_TIMER_UART1_PWM, frequency);
        if (err_freq != ESP_OK) {
            USBSerial.printf("[UART1] PWM frequency set failed: %s\n", esp_err_to_name(err_freq));
            return false;
        }

        esp_err_t err_duty = mcpwm_set_duty(MCPWM_UNIT_UART1_PWM, MCPWM_TIMER_UART1_PWM,
                                            MCPWM_GEN_UART1_PWM, duty);
        if (err_duty != ESP_OK) {
            USBSerial.printf("[UART1] PWM duty set failed: %s\n", esp_err_to_name(err_duty));
            return false;
        }

        // CRITICAL: Read actual register values after mcpwm_set_frequency()
        // ESP-IDF may use different prescaler/period calculation than ours!
        uint32_t cfg0_actual = MCPWM1.timer[0].timer_cfg0.val;
        uint32_t actual_prescaler = (cfg0_actual & 0xFF);
        uint32_t actual_period = ((cfg0_actual >> 8) & 0xFFFF);

        USBSerial.printf("[UART1] 📖 Register after mcpwm_set_frequency(): prescaler=%u, period=%u\n",
                     actual_prescaler, actual_period);

        // Update stored values with ACTUAL register values (not calculated values!)
        pwmPrescaler = actual_prescaler;
        pwmPeriod = actual_period;
        pwmFrequency = frequency;
        pwmDuty = duty;

        USBSerial.printf("[UART1] PWM updated: %u Hz, %.1f%% (prescaler=%u, period=%u)\n",
                     frequency, duty, pwmPrescaler, pwmPeriod);
    }

    USBSerial.println("[UART1] 🏁 setPWMFrequencyAndDuty() RETURN TRUE");
    USBSerial.flush();
    return true;
}

void UART1Mux::setPWMEnabled(bool enable) {
    if (currentMode != MODE_PWM_RPM) {
        return;
    }

    pwmEnabled = enable;

    if (enable) {
        // Start MCPWM timer
        mcpwm_start(MCPWM_UNIT_UART1_PWM, MCPWM_TIMER_UART1_PWM);
    } else {
        // Stop MCPWM timer
        mcpwm_stop(MCPWM_UNIT_UART1_PWM, MCPWM_TIMER_UART1_PWM);
    }
}

// ============================================================================
// MCPWM Capture ISR Callback
// ============================================================================

bool IRAM_ATTR UART1Mux::captureCallback(mcpwm_unit_t mcpwm,
                                          mcpwm_capture_channel_id_t cap_channel,
                                          const cap_event_data_t *edata,
                                          void *user_data) {
    // This runs in ISR context - must be fast!
    static uint32_t lastCapture = 0;
    uint32_t currentCapture = edata->cap_value;

    if (lastCapture != 0) {
        // Calculate period between captures (in timer ticks)
        if (currentCapture > lastCapture) {
            capturePeriod = currentCapture - lastCapture;
        } else {
            // Handle timer overflow (32-bit counter)
            capturePeriod = (UINT32_MAX - lastCapture) + currentCapture + 1;
        }
        newCaptureAvailable = true;
    }

    lastCapture = currentCapture;
    lastCaptureTime = millis();  // Track last valid capture time

    return false;  // Don't wake higher priority task
}

void UART1Mux::updateRPMFrequency() {
    if (currentMode != MODE_PWM_RPM) {
        rpmFrequency = 0.0;
        return;
    }

    // Check if new capture data is available
    if (newCaptureAvailable) {
        newCaptureAvailable = false;

        // Calculate frequency from captured period
        // Formula: frequency = MCPWM_CAPTURE_CLK / period
        // MCPWM_CAPTURE_CLK = 80,000,000 Hz (80 MHz APB clock)
        if (capturePeriod > 0) {
            rpmFrequency = 80000000.0 / (float)capturePeriod;
            lastRPMUpdate = lastCaptureTime;
        }
    }

    // Check for signal timeout (no capture in last 500ms)
    unsigned long now = millis();
    if ((now - lastRPMUpdate) > 500) {
        rpmFrequency = 0.0;  // Signal lost
    }
}

bool UART1Mux::hasRPMSignal() const {
    if (currentMode != MODE_PWM_RPM) {
        return false;
    }

    // Signal detected if frequency > 0 and updated within last 500ms
    return (rpmFrequency > 0.0) && ((millis() - lastRPMUpdate) < 500);
}

// ============================================================================
// Status and Diagnostics
// ============================================================================

void UART1Mux::getUARTStatistics(uint32_t* txBytes, uint32_t* rxBytes, uint32_t* errors) {
    if (txBytes) *txBytes = uartTxBytes;
    if (rxBytes) *rxBytes = uartRxBytes;
    if (errors) *errors = uartErrors;
}

void UART1Mux::resetUARTStatistics() {
    uartTxBytes = 0;
    uartRxBytes = 0;
    uartErrors = 0;
}

// ============================================================================
// Private Helper Functions
// ============================================================================

bool UART1Mux::initUART() {
    uart_config_t uart_config = {
        .baud_rate = (int)uartBaudRate,
        .data_bits = uartDataBits,
        .parity = uartParity,
        .stop_bits = uartStopBits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t err = uart_param_config(uartNum, &uart_config);
    if (err != ESP_OK) {
        return false;
    }

    err = uart_set_pin(uartNum, PIN_UART1_TX, PIN_UART1_RX,
                      UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return false;
    }

    // Configure TX pin with internal pull-up (as requested)
    gpio_set_pull_mode((gpio_num_t)PIN_UART1_TX, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)PIN_UART1_RX, GPIO_PULLUP_ONLY);

    err = uart_driver_install(uartNum, 2048, 1024, 0, NULL, 0);
    if (err != ESP_OK) {
        return false;
    }

    return true;
}

bool UART1Mux::initPWM() {
    // Initialize MCPWM for PWM output (replaces LEDC)
    // Step 1: Configure GPIO for MCPWM
    mcpwm_gpio_init(MCPWM_UNIT_UART1_PWM, MCPWM0A, PIN_UART1_TX);

    // Step 2: Configure MCPWM clock source (CRITICAL: Use 160MHz PLL_CLK, not APB)
    // ESP32-S3 MCPWM clock options:
    // - MCPWM_TIMER_CLK_SRC_PLL160M: 160 MHz (recommended for high frequency)
    // But this requires ESP-IDF 5.x API, for ESP-IDF 4.x we use default APB_CLK

    // Step 3: Configure MCPWM parameters
    mcpwm_config_t pwm_config;
    pwm_config.frequency = pwmFrequency;    // Frequency in Hz
    pwm_config.cmpr_a = pwmDuty;            // Duty cycle of PWMxA (0.0 - 100.0)
    pwm_config.cmpr_b = 0;                  // Duty cycle of PWMxB (not used)
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;  // Active high
    pwm_config.counter_mode = MCPWM_UP_COUNTER;

    // Step 4: Initialize MCPWM
    esp_err_t err = mcpwm_init(MCPWM_UNIT_UART1_PWM, MCPWM_TIMER_UART1_PWM, &pwm_config);
    if (err != ESP_OK) {
        USBSerial.printf("[UART1] ❌ MCPWM PWM init failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // Step 4: Set duty type (active high) - ensures shadow register mode is enabled
    // This only needs to be called once during initialization, not during runtime updates
    mcpwm_set_duty_type(MCPWM_UNIT_UART1_PWM, MCPWM_TIMER_UART1_PWM,
                        MCPWM_GEN_UART1_PWM, MCPWM_DUTY_MODE_0);

    // Step 5: Read actual register values and detect clock frequency
    uint32_t cfg0_init = MCPWM1.timer[0].timer_cfg0.val;
    pwmPrescaler = (cfg0_init & 0xFF);
    pwmPeriod = ((cfg0_init >> 8) & 0xFFFF);

    // Calculate actual clock frequency based on register values and expected frequency
    // Expected: clock_freq / (prescaler × period) = pwmFrequency
    // Therefore: clock_freq = pwmFrequency × prescaler × period
    mcpwmClockFreq = pwmFrequency * pwmPrescaler * pwmPeriod;

    pwmEnabled = true;
    USBSerial.printf("[UART1] ✅ MCPWM PWM initialized (GPIO %d, %u Hz, %.1f%% duty)\n",
                 PIN_UART1_TX, pwmFrequency, pwmDuty);
    USBSerial.printf("[UART1] 📖 Actual register: prescaler=%u, period=%u\n", pwmPrescaler, pwmPeriod);
    USBSerial.printf("[UART1] 🔍 Detected MCPWM clock: %u Hz (%.1f MHz)\n",
                 mcpwmClockFreq, mcpwmClockFreq / 1000000.0f);

    if (mcpwmClockFreq < 10000000) {
        USBSerial.printf("[UART1] ⚠️  WARNING: Clock frequency seems too low! Expected ~80MHz\n");
        USBSerial.printf("[UART1] ⚠️  This will cause frequency errors in PWM output!\n");
    }
    return true;
}

bool UART1Mux::initRPM() {
    // Initialize MCPWM Capture for frequency measurement

    // Step 1: Route GPIO to MCPWM capture signal
    // CRITICAL: Must call mcpwm_gpio_init() to connect GPIO to MCPWM capture!
    // This creates the signal path: GPIO18 → MCPWM_UNIT_0 CAP1
    esp_err_t gpio_result = mcpwm_gpio_init(MCPWM_UNIT_UART1_RPM,  // MCPWM_UNIT_0
                                             MCPWM_CAP_1,            // Capture channel 1
                                             PIN_UART1_RX);          // GPIO 18

    if (gpio_result != ESP_OK) {
        USBSerial.printf("[UART1] ❌ MCPWM GPIO init failed: %s\n", esp_err_to_name(gpio_result));
        return false;
    }

    // Step 2: Set pull-up on capture input for stable idle state
    gpio_set_pull_mode((gpio_num_t)PIN_UART1_RX, GPIO_PULLUP_ONLY);

    // Step 3: Configure capture parameters
    mcpwm_capture_config_t cap_conf;
    cap_conf.cap_edge = MCPWM_POS_EDGE;        // Capture on rising edge
    cap_conf.cap_prescale = 1;                  // No prescaling (80 MHz)
    cap_conf.capture_cb = captureCallback;      // ISR callback
    cap_conf.user_data = nullptr;               // No user data needed

    // Step 4: Enable capture channel
    esp_err_t result = mcpwm_capture_enable_channel(MCPWM_UNIT_UART1_RPM,
                                                     MCPWM_CAP_UART1_RPM,
                                                     &cap_conf);

    if (result == ESP_OK) {
        // Initialize state variables
        capturePeriod = 0;
        newCaptureAvailable = false;
        lastCaptureTime = millis();
        lastRPMUpdate = millis();
        rpmFrequency = 0.0;

        USBSerial.printf("[UART1] ✅ MCPWM Capture initialized:\n");
        USBSerial.printf("  - Unit: MCPWM_UNIT_%d\n", MCPWM_UNIT_UART1_RPM);
        USBSerial.printf("  - Channel: CAP%d\n", (MCPWM_CAP_UART1_RPM == MCPWM_SELECT_CAP1) ? 1 : 0);
        USBSerial.printf("  - GPIO: %d (RX1)\n", PIN_UART1_RX);
        USBSerial.printf("  - Edge: Rising, Clock: 80 MHz\n");
        return true;
    }

    USBSerial.printf("[UART1] ❌ MCPWM Capture enable failed: %s\n", esp_err_to_name(result));
    return false;
}

void UART1Mux::deinitUART() {
    uart_driver_delete(uartNum);
}

void UART1Mux::deinitPWM() {
    // Stop MCPWM timer
    mcpwm_stop(MCPWM_UNIT_UART1_PWM, MCPWM_TIMER_UART1_PWM);
    pwmEnabled = false;
}

void UART1Mux::deinitRPM() {
    // Disable MCPWM Capture channel
    mcpwm_capture_disable_channel(MCPWM_UNIT_UART1_RPM, MCPWM_CAP_UART1_RPM);

    // Reset state variables
    capturePeriod = 0;
    newCaptureAvailable = false;
    rpmFrequency = 0.0;
}

void UART1Mux::releasePins() {
    // Reset GPIO to input mode (high-impedance)
    gpio_reset_pin((gpio_num_t)PIN_UART1_TX);
    gpio_reset_pin((gpio_num_t)PIN_UART1_RX);
}

bool UART1Mux::validateUARTConfig(uint32_t baudRate, uart_stop_bits_t stopBits,
                                  uart_parity_t parity, uart_word_length_t dataBits) {
    if (baudRate < 2400 || baudRate > 1500000) {
        USBSerial.printf("[UART1] Invalid baud rate: %u\n", baudRate);
        return false;
    }

    if (stopBits < UART_STOP_BITS_1 || stopBits > UART_STOP_BITS_2) {
        return false;
    }

    if (parity < UART_PARITY_DISABLE || parity > UART_PARITY_ODD) {
        return false;
    }

    if (dataBits < UART_DATA_5_BITS || dataBits > UART_DATA_8_BITS) {
        return false;
    }

    return true;
}

bool UART1Mux::validatePWMFrequency(uint32_t frequency) {
    if (frequency < 1 || frequency > 500000) {
        USBSerial.printf("[UART1] Invalid PWM frequency: %u (valid: 1-500000 Hz)\n", frequency);
        return false;
    }
    return true;
}

// ============================================================================
// Motor Control Functions
// ============================================================================

bool UART1Mux::setPolePairs(uint32_t poles) {
    if (poles < 1 || poles > 12) {
        USBSerial.printf("[UART1] Invalid pole pairs: %u (valid: 1-12)\n", poles);
        return false;
    }
    polePairs = poles;
    return true;
}

bool UART1Mux::setMaxFrequency(uint32_t freq) {
    if (freq < 10 || freq > 500000) {
        USBSerial.printf("[UART1] Invalid max frequency: %u (valid: 10-500000 Hz)\n", freq);
        return false;
    }
    maxFrequency = freq;
    return true;
}

float UART1Mux::getCalculatedRPM() const {
    if (currentMode != MODE_PWM_RPM) {
        return 0.0;
    }
    // RPM = (frequency × 60) / pole_pairs
    return (rpmFrequency * 60.0) / (float)polePairs;
}

// ============================================================================
// Settings Persistence
// ============================================================================

bool UART1Mux::saveSettings() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        USBSerial.println("[UART1] Failed to open NVS for saving");
        return false;
    }

    prefs.putUInt("pwmFreq", pwmFrequency);
    prefs.putFloat("pwmDuty", pwmDuty);
    prefs.putUInt("polePairs", polePairs);
    prefs.putUInt("maxFreq", maxFrequency);
    prefs.putUInt("uartBaud", uartBaudRate);

    prefs.end();
    USBSerial.println("[UART1] Settings saved to NVS");
    return true;
}

bool UART1Mux::loadSettings() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // Read-only
        USBSerial.println("[UART1] No saved settings found, using defaults");
        return false;
    }

    pwmFrequency = prefs.getUInt("pwmFreq", 1000);
    pwmDuty = prefs.getFloat("pwmDuty", 50.0);
    polePairs = prefs.getUInt("polePairs", 2);
    maxFrequency = prefs.getUInt("maxFreq", 100000);
    uartBaudRate = prefs.getUInt("uartBaud", 115200);

    prefs.end();
    USBSerial.println("[UART1] Settings loaded from NVS");
    return true;
}

void UART1Mux::resetToDefaults() {
    pwmFrequency = 1000;
    pwmDuty = 50.0;
    polePairs = 2;
    maxFrequency = 100000;
    uartBaudRate = 115200;

    USBSerial.println("[UART1] Settings reset to factory defaults");
}

// ============================================================================
// Debug/Test Functions
// ============================================================================

void UART1Mux::initPWMChangePulse() {
    // Initialize GPIO 12 as output for PWM parameter change pulse
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_PWM_CHANGE_PULSE);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Set initial state to LOW
    gpio_set_level((gpio_num_t)PIN_PWM_CHANGE_PULSE, 0);

    USBSerial.printf("[UART1] PWM change pulse initialized on GPIO %d\n", PIN_PWM_CHANGE_PULSE);
}

void UART1Mux::outputPWMChangePulse() {
    // NON-BLOCKING PWM change marker using GPIO toggle
    //
    // Instead of generating a pulse with delay (which blocks and causes PWM glitches),
    // we simply toggle GPIO12 state each time PWM parameters change.
    //
    // Benefits:
    // - Zero blocking time (no delayMicroseconds)
    // - Does not interfere with MCPWM operation
    // - Oscilloscope can trigger on rising or falling edge
    // - Each PWM change creates a visible edge
    //
    // Usage: Trigger oscilloscope on either edge of GPIO12 to capture PWM changes

    pwmChangePulseState = !pwmChangePulseState;
    gpio_set_level((gpio_num_t)PIN_PWM_CHANGE_PULSE, pwmChangePulseState ? 1 : 0);
}

// ============================================================================
// PWM Low-Level Register Manipulation
// ============================================================================

void UART1Mux::calculatePWMParameters(uint32_t frequency, uint32_t& prescaler, uint32_t& period) {
    // ESP32-S3 MCPWM uses APB clock: 80 MHz
    // Formula: output_freq = APB_CLK / (prescaler * period)
    // We want: prescaler * period = APB_CLK / output_freq

    const uint32_t APB_CLK = 80000000;  // 80 MHz
    uint32_t target_ticks = APB_CLK / frequency;

    // Find optimal prescaler and period combination
    // Goal: Maximize period for better resolution while keeping prescaler in range (1-256)

    // Start with prescaler = 1 for maximum resolution
    prescaler = 1;
    period = target_ticks;

    // If period exceeds 65535 (16-bit limit), increase prescaler
    while (period > 65535 && prescaler < 256) {
        prescaler++;
        period = target_ticks / prescaler;
    }

    // Ensure period is at least 2 (minimum valid value)
    if (period < 2) {
        period = 2;
    }
}

void UART1Mux::updatePWMRegistersDirectly(uint32_t period, float duty) {
    // UNIFIED SHADOW REGISTER UPDATE STRATEGY
    //
    // Use shadow register mode for BOTH period and duty:
    // 1. Period: Direct register access with PERIOD_UPMETHOD [24] = 1 (TEZ sync)
    // 2. Duty: mcpwm_set_duty() API (already uses TEZ sync internally)
    //
    // Both updates synchronized to TEZ (Timer Equals Zero) → Completely glitch-free!
    //
    // Register details:
    // - timer_cfg0 [31:0]: prescaler[7:0], period[23:8], period_upmethod[24]

    // Critical section for atomic register updates
    taskENTER_CRITICAL(&mux);

    // ===== Update Period (if changed) =====
    if (period != pwmPeriod) {
        // CRITICAL: Must write BOTH prescaler and period together!
        // The timer_cfg0 register contains:
        //   bits [7:0]  = prescaler
        //   bits [23:8] = period
        //   bit  [24]   = period_upmethod (1 = shadow register mode)

        // Read register BEFORE write for debugging
        uint32_t cfg0_before = MCPWM1.timer[0].timer_cfg0.val;
        USBSerial.printf("[UART1] 📖 BEFORE: cfg0=0x%08X, prescaler=%u, period=%u\n",
                     cfg0_before, (cfg0_before & 0xFF), ((cfg0_before >> 8) & 0xFFFF));

        // Build the complete register value with prescaler + period + shadow mode
        uint32_t cfg0_val = (pwmPrescaler & 0xFF)      // Prescaler [7:0]
                          | (period << 8)               // Period [23:8]
                          | (1 << 24);                  // Shadow mode [24]

        USBSerial.printf("[UART1] 🔧 Writing: prescaler=%u, period=%u, cfg0_val=0x%08X\n",
                     pwmPrescaler, period, cfg0_val);

        // Write complete value to register
        MCPWM1.timer[0].timer_cfg0.val = cfg0_val;

        // Read register AFTER write to verify
        uint32_t cfg0_after = MCPWM1.timer[0].timer_cfg0.val;
        USBSerial.printf("[UART1] 📖 AFTER:  cfg0=0x%08X, prescaler=%u, period=%u\n",
                     cfg0_after, (cfg0_after & 0xFF), ((cfg0_after >> 8) & 0xFFFF));

        // Verify calculation
        uint32_t calculated_freq = 80000000 / (pwmPrescaler * period);
        USBSerial.printf("[UART1] 🧮 Expected frequency: 80MHz / (%u × %u) = %u Hz\n",
                     pwmPrescaler, period, calculated_freq);

        // Update stored period value
        pwmPeriod = period;
    }

    taskEXIT_CRITICAL(&mux);

    // ===== Update Duty Cycle =====
    // Use ESP-IDF API (TEZ-synchronized, shadow register mode)
    mcpwm_set_duty(MCPWM_UNIT_UART1_PWM, MCPWM_TIMER_UART1_PWM, MCPWM_GEN_A, duty);

    // Update stored duty value
    pwmDuty = duty;
}
