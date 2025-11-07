#include "MotorControl.h"

// Static member initialization
volatile uint32_t MotorControl::capturePeriod = 0;
volatile bool MotorControl::newCaptureAvailable = false;
volatile uint32_t MotorControl::lastCaptureTime = 0;

MotorControl::MotorControl() {
    // Constructor - initialization happens in begin()
    // Initialize RPM filter buffer
    for (uint8_t i = 0; i < MAX_FILTER_SIZE; i++) {
        rpmFilterBuffer[i] = 0.0f;
    }
}

bool MotorControl::begin(MotorSettings* settings) {
    if (!settings) {
        Serial.println("❌ MotorControl::begin() - NULL settings pointer!");
        return false;
    }

    pSettings = settings;
    initTime = millis();

    // Configure pulse output pin
    pinMode(PULSE_OUTPUT_PIN, OUTPUT);
    digitalWrite(PULSE_OUTPUT_PIN, LOW);

    // Initialize PWM output
    if (!initPWM()) {
        Serial.println("❌ Failed to initialize MCPWM PWM output");
        return false;
    }

    // Initialize tachometer capture
    if (!initCapture()) {
        Serial.println("⚠️ Failed to initialize MCPWM Capture (tachometer may not work)");
        // Continue anyway - PWM still works
    }

    // Apply saved settings
    setPWMFrequency(pSettings->frequency);
    setPWMDuty(pSettings->duty);

    Serial.println("✅ Motor control initialized");
    Serial.printf("  PWM Output: GPIO %d (MCPWM%d Unit %d)\n",
                  PWM_OUTPUT_PIN, PWM_MCPWM_UNIT, PWM_MCPWM_TIMER);
    Serial.printf("  Tachometer: GPIO %d (MCPWM%d CAP%d)\n",
                  TACHOMETER_INPUT_PIN, CAP_MCPWM_UNIT, CAP_SIGNAL);
    Serial.printf("  Pulse Out: GPIO %d\n", PULSE_OUTPUT_PIN);
    Serial.printf("  Initial Frequency: %d Hz\n", pSettings->frequency);
    Serial.printf("  Initial Duty: %.1f%%\n", pSettings->duty);

    return true;
}

void MotorControl::end() {
    // Stop PWM
    if (mcpwmInitialized) {
        mcpwm_stop(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER);
        mcpwmInitialized = false;
    }

    // Disable capture
    if (captureInitialized) {
        // Note: ESP-IDF doesn't have a direct capture disable function
        // The capture will be reconfigured on next begin()
        captureInitialized = false;
    }

    currentFrequency = 0;
    currentDuty = 0.0;
    currentRPM = 0.0;
    currentInputFrequency = 0.0;
}

bool MotorControl::initPWM() {
    // Configure GPIO as MCPWM output
    mcpwm_gpio_init(PWM_MCPWM_UNIT, PWM_MCPWM_GEN, PWM_OUTPUT_PIN);

    // Configure MCPWM parameters
    mcpwm_config_t pwm_config;
    pwm_config.frequency = pSettings->frequency;
    pwm_config.cmpr_a = pSettings->duty;
    pwm_config.cmpr_b = 0;  // Not used
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;  // Active high
    pwm_config.counter_mode = MCPWM_UP_COUNTER;

    // Initialize MCPWM
    esp_err_t result = mcpwm_init(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, &pwm_config);

    if (result == ESP_OK) {
        mcpwmInitialized = true;

        // Set initial frequency and duty
        mcpwm_set_frequency(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, pSettings->frequency);
        mcpwm_set_duty(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, MCPWM_OPR_A, pSettings->duty);
        mcpwm_set_duty_type(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);

        // Start the PWM timer
        mcpwm_start(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER);

        currentFrequency = pSettings->frequency;
        currentDuty = pSettings->duty;

        return true;
    }

    Serial.printf("❌ MCPWM init failed: %s\n", esp_err_to_name(result));
    return false;
}

bool MotorControl::initCapture() {
    // Configure GPIO as MCPWM Capture input
    mcpwm_gpio_init(CAP_MCPWM_UNIT, MCPWM_CAP_0, TACHOMETER_INPUT_PIN);

    // Setup capture configuration
    mcpwm_capture_config_t cap_conf;
    cap_conf.cap_edge = MCPWM_POS_EDGE;        // Capture on rising edge
    cap_conf.cap_prescale = 1;                  // No prescaling (80 MHz)
    cap_conf.capture_cb = captureCallback;      // ISR callback
    cap_conf.user_data = nullptr;               // No user data needed

    // Enable capture channel
    esp_err_t result = mcpwm_capture_enable_channel(CAP_MCPWM_UNIT,
                                                     CAP_SIGNAL,
                                                     &cap_conf);

    if (result == ESP_OK) {
        captureInitialized = true;
        Serial.printf("✅ MCPWM Capture initialized (GPIO %d, rising edge, %d MHz)\n",
                     TACHOMETER_INPUT_PIN, MCPWM_CAP_TIMER_CLK / 1000000);
        return true;
    }

    Serial.printf("❌ MCPWM Capture init failed: %s\n", esp_err_to_name(result));
    return false;
}

bool IRAM_ATTR MotorControl::captureCallback(mcpwm_unit_t mcpwm,
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

bool MotorControl::setPWMFrequency(uint32_t frequency) {
    // ALWAYS print to Serial (bypasses any response routing)
    Serial.printf("\n>>> setPWMFrequency called: %d Hz <<<\n", frequency);
    Serial.flush();
    
    // Validate range
    if (frequency < MotorLimits::MIN_FREQUENCY) {
        Serial.printf("⚠️ Frequency %d Hz too low, clamping to %d Hz\n",
                     frequency, MotorLimits::MIN_FREQUENCY);
        frequency = MotorLimits::MIN_FREQUENCY;
    }
    if (frequency > MotorLimits::MAX_FREQUENCY) {
        Serial.printf("⚠️ Frequency %d Hz too high, clamping to %d Hz\n",
                     frequency, MotorLimits::MAX_FREQUENCY);
        frequency = MotorLimits::MAX_FREQUENCY;
    }

    // Check if MCPWM is initialized
    if (!mcpwmInitialized) {
        Serial.println("❌ MCPWM not initialized!");
        return false;
    }

    // Skip if unchanged
    if (frequency == currentFrequency) {
        Serial.printf("⏭️  PWM frequency unchanged (%d Hz), skipping\n", frequency);
        return true;
    }

    Serial.printf("🔧 Setting PWM frequency from %d Hz to %d Hz...\n", currentFrequency, frequency);
    
    // Stop timer before reconfiguration
    mcpwm_stop(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER);
    
    Serial.printf("   Configuring MCPWM for %d Hz...\n", frequency);
    
    // Use standard API - works reliably up to 500 kHz
    esp_err_t result = mcpwm_set_frequency(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, frequency);
    
    Serial.printf("   mcpwm_set_frequency result: %s\n", esp_err_to_name(result));
    
    if (result == ESP_OK) {
        // Reapply duty cycle
        esp_err_t duty_result = mcpwm_set_duty(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, MCPWM_OPR_A, currentDuty);
        Serial.printf("   mcpwm_set_duty result: %s (duty: %.1f%%)\n", esp_err_to_name(duty_result), currentDuty);
        
        mcpwm_set_duty_type(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    }

    if (result == ESP_OK) {
        // Restart timer
        mcpwm_start(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER);
        
        currentFrequency = frequency;
        pSettings->frequency = frequency;

        Serial.printf("✅ PWM frequency set to: %d Hz (duty: %.1f%%)\n",
                     frequency, currentDuty);

        // Send pulse notification
        sendPulse();
        return true;
    }

    Serial.printf("❌ Failed to set PWM frequency to %d Hz: %s\n", frequency, esp_err_to_name(result));
    Serial.printf("   MCPWM may not support this frequency (max ~40 MHz)\n");
    
    // Restart timer anyway to maintain previous state
    mcpwm_start(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER);
    
    return false;
}

bool MotorControl::setPWMDuty(float duty) {
    // Validate range
    if (duty < MotorLimits::MIN_DUTY) {
        duty = MotorLimits::MIN_DUTY;
    }
    if (duty > MotorLimits::MAX_DUTY) {
        duty = MotorLimits::MAX_DUTY;
    }

    // Check if MCPWM is initialized
    if (!mcpwmInitialized) {
        Serial.println("❌ MCPWM not initialized!");
        return false;
    }

    // Skip if unchanged (with small tolerance for float comparison)
    if (fabs(duty - currentDuty) < 0.01) {
        Serial.printf("⏭️  PWM duty unchanged (%.1f%%), skipping\n", duty);
        return true;
    }

    // Apply new duty cycle
    esp_err_t result = mcpwm_set_duty(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER,
                                      MCPWM_OPR_A, duty);

    if (result == ESP_OK) {
        // Ensure duty is applied
        mcpwm_set_duty_type(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER,
                           MCPWM_OPR_A, MCPWM_DUTY_MODE_0);

        currentDuty = duty;
        pSettings->duty = duty;

        Serial.printf("✅ PWM duty set to: %.1f%% (freq: %d Hz)\n",
                     duty, currentFrequency);

        // Send pulse notification
        sendPulse();
        return true;
    }

    Serial.printf("❌ Failed to set PWM duty: %s\n", esp_err_to_name(result));
    return false;
}

uint32_t MotorControl::getPWMFrequency() const {
    return currentFrequency;
}

float MotorControl::getPWMDuty() const {
    return currentDuty;
}

bool MotorControl::setPolePairs(uint8_t pairs) {
    // Validate range
    if (pairs < MotorLimits::MIN_POLE_PAIRS) {
        pairs = MotorLimits::MIN_POLE_PAIRS;
    }
    if (pairs > MotorLimits::MAX_POLE_PAIRS) {
        pairs = MotorLimits::MAX_POLE_PAIRS;
    }

    // Update settings
    if (pSettings) {
        pSettings->polePairs = pairs;
        Serial.printf("✅ Motor pole pairs set to: %d\n", pairs);
        return true;
    }

    Serial.println("❌ Cannot set pole pairs - settings not initialized");
    return false;
}

uint8_t MotorControl::getPolePairs() const {
    if (pSettings) {
        return pSettings->polePairs;
    }
    return MotorDefaults::POLE_PAIRS;
}

void MotorControl::updateRPM() {
    if (!captureInitialized) {
        return;
    }

    // Check if new capture data is available
    if (newCaptureAvailable) {
        // Atomic read of capture period
        noInterrupts();
        uint32_t period = capturePeriod;
        newCaptureAvailable = false;
        interrupts();

        if (period > 0) {
            // Calculate input frequency from period
            // Frequency = Clock / Period
            currentInputFrequency = (float)MCPWM_CAP_TIMER_CLK / (float)period;

            // Calculate raw RPM from input frequency
            // RPM = (Frequency × 60) / Pole Pairs
            rawRPM = (currentInputFrequency * 60.0f) / (float)pSettings->polePairs;

            // Apply filtering
            currentRPM = applyRPMFilter(rawRPM);

            lastRPMUpdateTime = millis();
        } else {
            currentInputFrequency = 0.0f;
            rawRPM = 0.0f;
            currentRPM = 0.0f;
        }
    } else {
        // No new capture - check for timeout (1 second)
        unsigned long now = millis();
        if (now - lastCaptureTime > 1000 && lastCaptureTime > 0) {
            // No signal for 1 second - assume stopped
            rawRPM = 0.0f;
            currentRPM = 0.0f;
            currentInputFrequency = 0.0f;
        }
    }

    lastRPM = currentRPM;
}

float MotorControl::getCurrentRPM() const {
    return currentRPM;
}

float MotorControl::getRawRPM() const {
    return rawRPM;
}

float MotorControl::getInputFrequency() const {
    return currentInputFrequency;
}

bool MotorControl::isInitialized() const {
    return mcpwmInitialized;
}

bool MotorControl::isCaptureInitialized() const {
    return captureInitialized;
}

bool MotorControl::checkSafety() {
    if (!pSettings) {
        return false;
    }

    // Check for overspeed
    if (currentRPM > pSettings->maxSafeRPM && currentRPM > 0) {
        Serial.printf("⚠️ OVERSPEED DETECTED: %.0f RPM (max: %d RPM)\n",
                     currentRPM, pSettings->maxSafeRPM);
        return false;
    }

    // Check for stall (motor not responding to PWM)
    // If duty > 10% but RPM is very low or zero, motor may be stalled
    if (currentDuty > 10.0f && currentRPM < 100.0f &&
        (millis() - initTime) > 5000) {  // Allow 5 seconds startup time
        // This is a potential stall condition
        // Don't trigger emergency stop automatically - just warn
        // (Some motors may legitimately run very slow)
        if (millis() - lastRPMUpdateTime > 2000) {
            Serial.println("⚠️ Potential motor stall detected (duty > 10%, RPM < 100)");
        }
    }

    return true;
}

void MotorControl::emergencyStop() {
    if (mcpwmInitialized) {
        // Immediately set duty to 0%
        mcpwm_set_duty(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, MCPWM_OPR_A, 0.0);
        mcpwm_set_duty_type(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER,
                           MCPWM_OPR_A, MCPWM_DUTY_MODE_0);

        currentDuty = 0.0;
        pSettings->duty = 0.0;
        emergencyStopActive = true;

        // Print emergency stop message with RPM information
        float rpm = getCurrentRPM();
        uint32_t maxRPM = pSettings ? pSettings->maxSafeRPM : 0;
        Serial.println("⛔ EMERGENCY STOP ACTIVATED - Duty set to 0%");
        Serial.printf("   Current RPM: %.1f / Max Safe RPM: %u\n", rpm, maxRPM);
    }
}

bool MotorControl::isEmergencyStopActive() const {
    return emergencyStopActive;
}

void MotorControl::clearEmergencyStop() {
    emergencyStopActive = false;
    Serial.println("✅ Emergency stop cleared - Normal operation resumed");
}

void MotorControl::sendPulse() {
    digitalWrite(PULSE_OUTPUT_PIN, HIGH);
    delayMicroseconds(PULSE_WIDTH_US);
    digitalWrite(PULSE_OUTPUT_PIN, LOW);
}

unsigned long MotorControl::getUptime() const {
    return millis() - initTime;
}

// ============================================================================
// RPM Filtering Implementation
// ============================================================================

float MotorControl::applyRPMFilter(float rawValue) {
    if (!rpmFilterEnabled || rpmFilterSize == 1) {
        // Filter disabled or size = 1, return raw value
        return rawValue;
    }

    // Add new sample to circular buffer
    rpmFilterBuffer[rpmFilterIndex] = rawValue;
    rpmFilterIndex = (rpmFilterIndex + 1) % rpmFilterSize;

    // Track how many samples we have
    if (rpmFilterCount < rpmFilterSize) {
        rpmFilterCount++;
    }

    // Calculate moving average
    float sum = 0.0f;
    for (uint8_t i = 0; i < rpmFilterCount; i++) {
        sum += rpmFilterBuffer[i];
    }

    return sum / (float)rpmFilterCount;
}

void MotorControl::setRPMFilterSize(uint8_t windowSize) {
    // Clamp to valid range
    if (windowSize < 1) windowSize = 1;
    if (windowSize > MAX_FILTER_SIZE) windowSize = MAX_FILTER_SIZE;

    rpmFilterSize = windowSize;
    rpmFilterIndex = 0;
    rpmFilterCount = 0;

    // Clear buffer
    for (uint8_t i = 0; i < MAX_FILTER_SIZE; i++) {
        rpmFilterBuffer[i] = 0.0f;
    }

    Serial.printf("✅ RPM filter size set to: %d samples\n", windowSize);
}

uint8_t MotorControl::getRPMFilterSize() const {
    return rpmFilterSize;
}

// ============================================================================
// PWM Ramping Implementation
// ============================================================================

bool MotorControl::setPWMFrequencyRamped(uint32_t frequency, uint32_t rampTimeMs) {
    // Validate range
    if (frequency < MotorLimits::MIN_FREQUENCY) {
        frequency = MotorLimits::MIN_FREQUENCY;
    }
    if (frequency > MotorLimits::MAX_FREQUENCY) {
        frequency = MotorLimits::MAX_FREQUENCY;
    }

    if (!mcpwmInitialized) {
        Serial.println("❌ MCPWM not initialized!");
        return false;
    }

    // If ramp time is 0, set immediately
    if (rampTimeMs == 0) {
        return setPWMFrequency(frequency);
    }

    // Start ramping
    targetFrequency = frequency;
    frequencyStartValue = currentFrequency;
    frequencyRampStart = millis();
    frequencyRampDuration = rampTimeMs;
    frequencyRampActive = true;

    Serial.printf("🔄 Starting frequency ramp: %d Hz → %d Hz over %d ms\n",
                 currentFrequency, frequency, rampTimeMs);

    return true;
}

bool MotorControl::setPWMDutyRamped(float duty, uint32_t rampTimeMs) {
    // Validate range
    if (duty < MotorLimits::MIN_DUTY) {
        duty = MotorLimits::MIN_DUTY;
    }
    if (duty > MotorLimits::MAX_DUTY) {
        duty = MotorLimits::MAX_DUTY;
    }

    if (!mcpwmInitialized) {
        Serial.println("❌ MCPWM not initialized!");
        return false;
    }

    // If ramp time is 0, set immediately
    if (rampTimeMs == 0) {
        return setPWMDuty(duty);
    }

    // Start ramping
    targetDuty = duty;
    dutyStartValue = currentDuty;
    dutyRampStart = millis();
    dutyRampDuration = rampTimeMs;
    dutyRampActive = true;

    Serial.printf("🔄 Starting duty ramp: %.1f%% → %.1f%% over %d ms\n",
                 currentDuty, duty, rampTimeMs);

    return true;
}

void MotorControl::updateRamping() {
    if (!mcpwmInitialized) {
        return;
    }

    unsigned long now = millis();
    bool rampCompleted = false;

    // Update frequency ramping
    if (frequencyRampActive) {
        unsigned long elapsed = now - frequencyRampStart;

        if (elapsed >= frequencyRampDuration) {
            // Ramp complete - set final value
            setPWMFrequency(targetFrequency);
            frequencyRampActive = false;
            rampCompleted = true;
        } else {
            // Calculate intermediate value using linear interpolation
            float progress = (float)elapsed / (float)frequencyRampDuration;
            uint32_t newFrequency = frequencyStartValue +
                                   (uint32_t)((float)(targetFrequency - frequencyStartValue) * progress);

            // Apply without change detection (force update)
            mcpwm_set_frequency(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, newFrequency);
            currentFrequency = newFrequency;
        }
    }

    // Update duty ramping
    if (dutyRampActive) {
        unsigned long elapsed = now - dutyRampStart;

        if (elapsed >= dutyRampDuration) {
            // Ramp complete - set final value
            setPWMDuty(targetDuty);
            dutyRampActive = false;
            rampCompleted = true;
        } else {
            // Calculate intermediate value using linear interpolation
            float progress = (float)elapsed / (float)dutyRampDuration;
            float newDuty = dutyStartValue + (targetDuty - dutyStartValue) * progress;

            // Apply without change detection (force update)
            mcpwm_set_duty(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, MCPWM_OPR_A, newDuty);
            mcpwm_set_duty_type(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER,
                               MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
            currentDuty = newDuty;
        }
    }

    // Send pulse when ramp completes
    if (rampCompleted) {
        sendPulse();
    }
}

bool MotorControl::isRamping() const {
    return frequencyRampActive || dutyRampActive;
}

// ============================================================================
// Watchdog Timer Implementation
// ============================================================================

void MotorControl::feedWatchdog() {
    lastWatchdogFeed = millis();

    // Enable watchdog on first feed
    if (!watchdogEnabled) {
        watchdogEnabled = true;
        Serial.printf("✅ Watchdog timer enabled (timeout: %d ms)\n", WATCHDOG_TIMEOUT_MS);
    }
}

bool MotorControl::checkWatchdog() {
    if (!watchdogEnabled) {
        return true;  // Not enabled, always OK
    }

    unsigned long now = millis();
    unsigned long elapsed = now - lastWatchdogFeed;

    if (elapsed > WATCHDOG_TIMEOUT_MS) {
        Serial.printf("⚠️ WATCHDOG TIMEOUT: %lu ms since last feed (max: %d ms)\n",
                     elapsed, WATCHDOG_TIMEOUT_MS);
        return false;
    }

    return true;
}
