#include "BuzzerControl.h"

BuzzerControl::BuzzerControl() {
}

bool BuzzerControl::begin(uint32_t frequency, float duty) {
    if (initialized) {
        return true;
    }

    // Validate parameters
    if (!validateFrequency(frequency) || !validateDuty(duty)) {
        return false;
    }

    // Configure LEDC timer for buzzer
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,  // 10-bit resolution (0-1023)
        .timer_num = (ledc_timer_t)LEDC_TIMER_BUZZER,
        .freq_hz = frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        Serial.printf("[Buzzer] Timer config failed: %d\n", err);
        return false;
    }

    // Configure LEDC channel for buzzer
    ledc_channel_config_t channel_conf = {
        .gpio_num = PIN_BUZZER_PWM,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = (ledc_channel_t)LEDC_CHANNEL_BUZZER,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)LEDC_TIMER_BUZZER,
        .duty = 0,  // Start with buzzer off
        .hpoint = 0
    };

    err = ledc_channel_config(&channel_conf);
    if (err != ESP_OK) {
        Serial.printf("[Buzzer] Channel config failed: %d\n", err);
        return false;
    }

    currentFrequency = frequency;
    currentDuty = duty;
    buzzerEnabled = false;  // Start disabled
    initialized = true;

    Serial.printf("[Buzzer] Initialized: %u Hz, %.1f%% duty\n", frequency, duty);

    return true;
}

bool BuzzerControl::setFrequency(uint32_t frequency) {
    if (!initialized) {
        return false;
    }

    if (!validateFrequency(frequency)) {
        return false;
    }

    // Update LEDC timer frequency
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = (ledc_timer_t)LEDC_TIMER_BUZZER,
        .freq_hz = frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        Serial.printf("[Buzzer] Frequency set failed: %d\n", err);
        return false;
    }

    currentFrequency = frequency;

    // Re-apply duty cycle if buzzer is enabled
    if (buzzerEnabled) {
        uint32_t dutyValue = (uint32_t)((currentDuty / 100.0) * 1023.0);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_BUZZER, dutyValue);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_BUZZER);
    }

    return true;
}

bool BuzzerControl::setDuty(float duty) {
    if (!initialized) {
        return false;
    }

    if (!validateDuty(duty)) {
        return false;
    }

    currentDuty = duty;

    // Apply duty cycle if buzzer is enabled
    if (buzzerEnabled) {
        uint32_t dutyValue = (uint32_t)((duty / 100.0) * 1023.0);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_BUZZER, dutyValue);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_BUZZER);
    }

    return true;
}

void BuzzerControl::enable(bool enabled) {
    if (!initialized) {
        return;
    }

    buzzerEnabled = enabled;

    if (enabled) {
        // Set duty cycle to current value
        uint32_t dutyValue = (uint32_t)((currentDuty / 100.0) * 1023.0);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_BUZZER, dutyValue);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_BUZZER);
    } else {
        // Set duty cycle to 0 (silence)
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_BUZZER, 0);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_BUZZER);
    }
}

void BuzzerControl::beep(uint32_t frequency, uint32_t durationMs, float duty) {
    if (!initialized) {
        return;
    }

    // Save current settings
    uint32_t savedFreq = currentFrequency;
    float savedDuty = currentDuty;
    bool savedEnabled = buzzerEnabled;

    // Set beep parameters
    setFrequency(frequency);
    setDuty(duty);
    enable(true);

    // Wait for duration
    delay(durationMs);

    // Restore previous settings
    enable(false);
    setFrequency(savedFreq);
    setDuty(savedDuty);
    enable(savedEnabled);
}

void BuzzerControl::playMelody(const uint32_t* frequencies, const uint32_t* durations,
                               uint8_t count, float duty) {
    if (!initialized || frequencies == nullptr || durations == nullptr) {
        return;
    }

    // Save current settings
    uint32_t savedFreq = currentFrequency;
    float savedDuty = currentDuty;
    bool savedEnabled = buzzerEnabled;

    setDuty(duty);

    for (uint8_t i = 0; i < count; i++) {
        if (frequencies[i] == 0) {
            // Rest (silence)
            enable(false);
        } else {
            // Play tone
            setFrequency(frequencies[i]);
            enable(true);
        }

        delay(durations[i]);
    }

    // Restore previous settings
    enable(false);
    setFrequency(savedFreq);
    setDuty(savedDuty);
    enable(savedEnabled);
}

void BuzzerControl::stop() {
    enable(false);
}

bool BuzzerControl::validateFrequency(uint32_t frequency) {
    if (frequency < 10 || frequency > 20000) {
        Serial.printf("[Buzzer] Invalid frequency: %u (valid: 10-20000 Hz)\n", frequency);
        return false;
    }
    return true;
}

bool BuzzerControl::validateDuty(float duty) {
    if (duty < 0.0 || duty > 100.0) {
        Serial.printf("[Buzzer] Invalid duty: %.1f (valid: 0-100%%)\n", duty);
        return false;
    }
    return true;
}
