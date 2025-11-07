#include "UART1Mux.h"
#include "driver/gpio.h"

UART1Mux::UART1Mux() {
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
        Serial.println("[UART1] Failed to initialize UART mode");
        return false;
    }

    currentMode = MODE_UART;
    Serial.printf("[UART1] Switched to UART mode: %u baud\n", baudRate);

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
        Serial.println("[UART1] Failed to initialize PWM/RPM mode");
        disable();
        return false;
    }

    currentMode = MODE_PWM_RPM;
    Serial.println("[UART1] Switched to PWM/RPM mode");

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
        Serial.printf("[UART1] Reconfigure failed: %d\n", err);
        return false;
    }

    uartBaudRate = baudRate;
    uartStopBits = stopBits;
    uartParity = parity;
    uartDataBits = dataBits;

    Serial.printf("[UART1] Reconfigured: %u baud\n", baudRate);
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

    // Configure LEDC timer frequency
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,  // 13-bit resolution (0-8191)
        .timer_num = (ledc_timer_t)LEDC_TIMER_UART1_PWM,
        .freq_hz = frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        Serial.printf("[UART1] PWM frequency set failed: %d\n", err);
        return false;
    }

    pwmFrequency = frequency;

    // Re-apply duty cycle
    uint32_t dutyValue = (uint32_t)((pwmDuty / 100.0) * 8191.0);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_UART1_PWM, dutyValue);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_UART1_PWM);

    return true;
}

bool UART1Mux::setPWMDuty(float duty) {
    if (currentMode != MODE_PWM_RPM) {
        return false;
    }

    if (duty < 0.0 || duty > 100.0) {
        return false;
    }

    pwmDuty = duty;

    // Calculate duty value (13-bit: 0-8191)
    uint32_t dutyValue = (uint32_t)((duty / 100.0) * 8191.0);

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_UART1_PWM, dutyValue);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_UART1_PWM);

    return true;
}

void UART1Mux::setPWMEnabled(bool enable) {
    if (currentMode != MODE_PWM_RPM) {
        return;
    }

    pwmEnabled = enable;

    if (enable) {
        // Resume PWM
        ledc_timer_resume(LEDC_HIGH_SPEED_MODE, (ledc_timer_t)LEDC_TIMER_UART1_PWM);
    } else {
        // Pause PWM
        ledc_timer_pause(LEDC_HIGH_SPEED_MODE, (ledc_timer_t)LEDC_TIMER_UART1_PWM);
    }
}

void UART1Mux::updateRPMFrequency() {
    if (currentMode != MODE_PWM_RPM) {
        rpmFrequency = 0.0;
        return;
    }

    unsigned long now = millis();
    unsigned long elapsed = now - rpmMeasureStartTime;

    // Measurement window: 100ms
    if (elapsed >= 100) {
        // Read PCNT counter value
        int16_t count = 0;
        pcnt_get_counter_value(PCNT_UNIT_UART1_RPM, &count);

        // Calculate frequency (Hz)
        // count = pulses in 100ms
        // frequency = count * 10
        rpmFrequency = (float)abs(count) * 10.0;

        // Reset counter for next measurement
        pcnt_counter_clear(PCNT_UNIT_UART1_RPM);

        // Update timing
        rpmMeasureStartTime = now;
        lastRPMUpdate = now;
        lastPCNTCount = count;
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
    // Configure LEDC timer
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = (ledc_timer_t)LEDC_TIMER_UART1_PWM,
        .freq_hz = pwmFrequency,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        return false;
    }

    // Configure LEDC channel
    ledc_channel_config_t channel_conf = {
        .gpio_num = PIN_UART1_TX,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = (ledc_channel_t)LEDC_CHANNEL_UART1_PWM,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)LEDC_TIMER_UART1_PWM,
        .duty = (uint32_t)((pwmDuty / 100.0) * 8191.0),
        .hpoint = 0
    };

    err = ledc_channel_config(&channel_conf);
    if (err != ESP_OK) {
        return false;
    }

    pwmEnabled = true;
    return true;
}

bool UART1Mux::initRPM() {
    // Configure PCNT unit
    pcnt_config_t pcnt_config = {
        .pulse_gpio_num = PIN_UART1_RX,
        .ctrl_gpio_num = PCNT_PIN_NOT_USED,
        .lctrl_mode = PCNT_MODE_KEEP,
        .hctrl_mode = PCNT_MODE_KEEP,
        .pos_mode = PCNT_COUNT_INC,      // Count on rising edge
        .neg_mode = PCNT_COUNT_DIS,      // Don't count on falling edge
        .counter_h_lim = 32767,
        .counter_l_lim = -32768,
        .unit = PCNT_UNIT_UART1_RPM,
        .channel = PCNT_CHANNEL_0,
    };

    esp_err_t err = pcnt_unit_config(&pcnt_config);
    if (err != ESP_OK) {
        Serial.printf("[UART1] PCNT config failed: %d\n", err);
        return false;
    }

    // Set PCNT filter (13ns * 1023 ≈ 13us, filters glitches < 77kHz)
    pcnt_set_filter_value(PCNT_UNIT_UART1_RPM, 1023);
    pcnt_filter_enable(PCNT_UNIT_UART1_RPM);

    // Initialize counter
    pcnt_counter_pause(PCNT_UNIT_UART1_RPM);
    pcnt_counter_clear(PCNT_UNIT_UART1_RPM);
    pcnt_counter_resume(PCNT_UNIT_UART1_RPM);

    // Initialize timing
    rpmMeasureStartTime = millis();
    lastRPMUpdate = millis();
    rpmFrequency = 0.0;

    return true;
}

void UART1Mux::deinitUART() {
    uart_driver_delete(uartNum);
}

void UART1Mux::deinitPWM() {
    ledc_stop(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_UART1_PWM, 0);
    pwmEnabled = false;
}

void UART1Mux::deinitRPM() {
    pcnt_counter_pause(PCNT_UNIT_UART1_RPM);
    pcnt_counter_clear(PCNT_UNIT_UART1_RPM);
}

void UART1Mux::releasePins() {
    // Reset GPIO to input mode (high-impedance)
    gpio_reset_pin((gpio_num_t)PIN_UART1_TX);
    gpio_reset_pin((gpio_num_t)PIN_UART1_RX);
}

bool UART1Mux::validateUARTConfig(uint32_t baudRate, uart_stop_bits_t stopBits,
                                  uart_parity_t parity, uart_word_length_t dataBits) {
    if (baudRate < 2400 || baudRate > 1500000) {
        Serial.printf("[UART1] Invalid baud rate: %u\n", baudRate);
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
        Serial.printf("[UART1] Invalid PWM frequency: %u (valid: 1-500000 Hz)\n", frequency);
        return false;
    }
    return true;
}
