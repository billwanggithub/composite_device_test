#include "LEDPWMControl.h"

LEDPWMControl::LEDPWMControl() {
}

bool LEDPWMControl::begin(uint32_t frequency, float brightness) {
    if (initialized) {
        return true;
    }

    // Validate parameters
    if (!validateFrequency(frequency) || !validateBrightness(brightness)) {
        return false;
    }

    // Configure LEDC timer for LED
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,  // 10-bit resolution (0-1023)
        .timer_num = (ledc_timer_t)LEDC_TIMER_LED,
        .freq_hz = frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        Serial.printf("[LED_PWM] Timer config failed: %d\n", err);
        return false;
    }

    // Configure LEDC channel for LED
    ledc_channel_config_t channel_conf = {
        .gpio_num = PIN_LED_PWM,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = (ledc_channel_t)LEDC_CHANNEL_LED,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)LEDC_TIMER_LED,
        .duty = 0,  // Start with LED off
        .hpoint = 0
    };

    err = ledc_channel_config(&channel_conf);
    if (err != ESP_OK) {
        Serial.printf("[LED_PWM] Channel config failed: %d\n", err);
        return false;
    }

    currentFrequency = frequency;
    currentBrightness = brightness;
    ledEnabled = false;  // Start disabled
    initialized = true;

    Serial.printf("[LED_PWM] Initialized: %u Hz, %.1f%% brightness\n", frequency, brightness);

    return true;
}

bool LEDPWMControl::setFrequency(uint32_t frequency) {
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
        .timer_num = (ledc_timer_t)LEDC_TIMER_LED,
        .freq_hz = frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        Serial.printf("[LED_PWM] Frequency set failed: %d\n", err);
        return false;
    }

    currentFrequency = frequency;

    // Re-apply brightness if LED is enabled
    if (ledEnabled) {
        uint32_t dutyValue = (uint32_t)((currentBrightness / 100.0) * 1023.0);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_LED, dutyValue);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_LED);
    }

    return true;
}

bool LEDPWMControl::setBrightness(float brightness) {
    if (!initialized) {
        return false;
    }

    if (!validateBrightness(brightness)) {
        return false;
    }

    currentBrightness = brightness;

    // Apply brightness if LED is enabled
    if (ledEnabled) {
        uint32_t dutyValue = (uint32_t)((brightness / 100.0) * 1023.0);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_LED, dutyValue);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_LED);
    }

    return true;
}

void LEDPWMControl::enable(bool enabled) {
    if (!initialized) {
        return;
    }

    ledEnabled = enabled;

    if (enabled) {
        // Set duty cycle to current brightness
        uint32_t dutyValue = (uint32_t)((currentBrightness / 100.0) * 1023.0);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_LED, dutyValue);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_LED);
    } else {
        // Set duty cycle to 0 (off)
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_LED, 0);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)LEDC_CHANNEL_LED);
    }
}

void LEDPWMControl::fadeTo(float targetBrightness, uint32_t fadeTimeMs, uint16_t steps) {
    if (!initialized || !validateBrightness(targetBrightness)) {
        return;
    }

    if (steps == 0) {
        steps = 1;
    }

    float startBrightness = currentBrightness;
    float brightnessStep = (targetBrightness - startBrightness) / steps;
    uint32_t delayPerStep = fadeTimeMs / steps;

    enable(true);  // Ensure LED is enabled

    for (uint16_t i = 0; i < steps; i++) {
        float brightness = startBrightness + (brightnessStep * i);
        setBrightness(brightness);
        delay(delayPerStep);
    }

    // Set final brightness
    setBrightness(targetBrightness);
}

void LEDPWMControl::blink(uint32_t onTimeMs, uint32_t offTimeMs, uint16_t cycles) {
    if (!initialized) {
        return;
    }

    // Save current state
    float savedBrightness = currentBrightness;
    bool savedEnabled = ledEnabled;

    // Blink cycles
    uint16_t count = (cycles == 0) ? 0xFFFF : cycles;
    for (uint16_t i = 0; i < count; i++) {
        enable(true);
        delay(onTimeMs);
        enable(false);
        delay(offTimeMs);
    }

    // Restore previous state
    currentBrightness = savedBrightness;
    enable(savedEnabled);
}

void LEDPWMControl::stop() {
    enable(false);
}

bool LEDPWMControl::validateFrequency(uint32_t frequency) {
    if (frequency < 100 || frequency > 20000) {
        Serial.printf("[LED_PWM] Invalid frequency: %u (valid: 100-20000 Hz)\n", frequency);
        return false;
    }
    return true;
}

bool LEDPWMControl::validateBrightness(float brightness) {
    if (brightness < 0.0 || brightness > 100.0) {
        Serial.printf("[LED_PWM] Invalid brightness: %.1f (valid: 0-100%%)\n", brightness);
        return false;
    }
    return true;
}
