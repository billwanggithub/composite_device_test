#include "PeripheralSettings.h"

// NVS namespace
const char* PeripheralSettingsManager::NVS_NAMESPACE = "peripherals";

// NVS keys (max 15 characters)
const char* PeripheralSettingsManager::KEY_UART1_MODE = "u1_mode";
const char* PeripheralSettingsManager::KEY_UART1_BAUD = "u1_baud";
const char* PeripheralSettingsManager::KEY_UART1_PWM_FREQ = "u1_pwm_freq";
const char* PeripheralSettingsManager::KEY_UART1_PWM_DUTY = "u1_pwm_duty";
const char* PeripheralSettingsManager::KEY_UART1_PWM_EN = "u1_pwm_en";
const char* PeripheralSettingsManager::KEY_UART2_BAUD = "u2_baud";
const char* PeripheralSettingsManager::KEY_BUZZER_FREQ = "buz_freq";
const char* PeripheralSettingsManager::KEY_BUZZER_DUTY = "buz_duty";
const char* PeripheralSettingsManager::KEY_BUZZER_EN = "buz_en";
const char* PeripheralSettingsManager::KEY_LED_FREQ = "led_freq";
const char* PeripheralSettingsManager::KEY_LED_BRIGHT = "led_bright";
const char* PeripheralSettingsManager::KEY_LED_EN = "led_en";
const char* PeripheralSettingsManager::KEY_RELAY_STATE = "relay_state";
const char* PeripheralSettingsManager::KEY_GPIO_STATE = "gpio_state";
const char* PeripheralSettingsManager::KEY_KEY_MODE = "key_mode";
const char* PeripheralSettingsManager::KEY_KEY_DUTY_STEP = "key_d_step";
const char* PeripheralSettingsManager::KEY_KEY_FREQ_STEP = "key_f_step";
const char* PeripheralSettingsManager::KEY_KEY_CTRL_EN = "key_ctrl_en";

bool PeripheralSettingsManager::begin() {
    if (initialized) {
        return true;
    }

    // Open NVS in read-write mode
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        Serial.println("[PeripheralSettings] Failed to open NVS");
        return false;
    }

    initialized = true;
    Serial.println("[PeripheralSettings] Initialized");
    return true;
}

bool PeripheralSettingsManager::load() {
    if (!initialized) {
        return false;
    }

    // Load UART1 settings
    settings.uart1Mode = preferences.getUChar(KEY_UART1_MODE, PeripheralDefaults::UART1_MODE);
    settings.uart1Baud = preferences.getUInt(KEY_UART1_BAUD, PeripheralDefaults::UART1_BAUD);
    settings.uart1PwmFreq = preferences.getUInt(KEY_UART1_PWM_FREQ, PeripheralDefaults::UART1_PWM_FREQ);
    settings.uart1PwmDuty = preferences.getFloat(KEY_UART1_PWM_DUTY, PeripheralDefaults::UART1_PWM_DUTY);
    settings.uart1PwmEnabled = preferences.getBool(KEY_UART1_PWM_EN, PeripheralDefaults::UART1_PWM_ENABLED);

    // Load UART2 settings
    settings.uart2Baud = preferences.getUInt(KEY_UART2_BAUD, PeripheralDefaults::UART2_BAUD);

    // Load Buzzer settings
    settings.buzzerFreq = preferences.getUInt(KEY_BUZZER_FREQ, PeripheralDefaults::BUZZER_FREQ);
    settings.buzzerDuty = preferences.getFloat(KEY_BUZZER_DUTY, PeripheralDefaults::BUZZER_DUTY);
    settings.buzzerEnabled = preferences.getBool(KEY_BUZZER_EN, PeripheralDefaults::BUZZER_ENABLED);

    // Load LED PWM settings
    settings.ledPwmFreq = preferences.getUInt(KEY_LED_FREQ, PeripheralDefaults::LED_PWM_FREQ);
    settings.ledBrightness = preferences.getFloat(KEY_LED_BRIGHT, PeripheralDefaults::LED_BRIGHTNESS);
    settings.ledEnabled = preferences.getBool(KEY_LED_EN, PeripheralDefaults::LED_ENABLED);

    // Load Relay settings
    settings.relayState = preferences.getBool(KEY_RELAY_STATE, PeripheralDefaults::RELAY_STATE);

    // Load GPIO settings
    settings.gpioState = preferences.getBool(KEY_GPIO_STATE, PeripheralDefaults::GPIO_STATE);

    // Load User Keys settings
    settings.keyControlAdjustDuty = preferences.getBool(KEY_KEY_MODE, PeripheralDefaults::KEY_ADJUST_DUTY);
    settings.keyDutyStep = preferences.getFloat(KEY_KEY_DUTY_STEP, PeripheralDefaults::KEY_DUTY_STEP);
    settings.keyFreqStep = preferences.getUInt(KEY_KEY_FREQ_STEP, PeripheralDefaults::KEY_FREQ_STEP);
    settings.keyControlEnabled = preferences.getBool(KEY_KEY_CTRL_EN, PeripheralDefaults::KEY_CONTROL_ENABLED);

    Serial.println("[PeripheralSettings] Settings loaded from NVS");
    return true;
}

bool PeripheralSettingsManager::save() {
    if (!initialized) {
        return false;
    }

    // Save UART1 settings
    preferences.putUChar(KEY_UART1_MODE, settings.uart1Mode);
    preferences.putUInt(KEY_UART1_BAUD, settings.uart1Baud);
    preferences.putUInt(KEY_UART1_PWM_FREQ, settings.uart1PwmFreq);
    preferences.putFloat(KEY_UART1_PWM_DUTY, settings.uart1PwmDuty);
    preferences.putBool(KEY_UART1_PWM_EN, settings.uart1PwmEnabled);

    // Save UART2 settings
    preferences.putUInt(KEY_UART2_BAUD, settings.uart2Baud);

    // Save Buzzer settings
    preferences.putUInt(KEY_BUZZER_FREQ, settings.buzzerFreq);
    preferences.putFloat(KEY_BUZZER_DUTY, settings.buzzerDuty);
    preferences.putBool(KEY_BUZZER_EN, settings.buzzerEnabled);

    // Save LED PWM settings
    preferences.putUInt(KEY_LED_FREQ, settings.ledPwmFreq);
    preferences.putFloat(KEY_LED_BRIGHT, settings.ledBrightness);
    preferences.putBool(KEY_LED_EN, settings.ledEnabled);

    // Save Relay settings
    preferences.putBool(KEY_RELAY_STATE, settings.relayState);

    // Save GPIO settings
    preferences.putBool(KEY_GPIO_STATE, settings.gpioState);

    // Save User Keys settings
    preferences.putBool(KEY_KEY_MODE, settings.keyControlAdjustDuty);
    preferences.putFloat(KEY_KEY_DUTY_STEP, settings.keyDutyStep);
    preferences.putUInt(KEY_KEY_FREQ_STEP, settings.keyFreqStep);
    preferences.putBool(KEY_KEY_CTRL_EN, settings.keyControlEnabled);

    Serial.println("[PeripheralSettings] Settings saved to NVS");
    return true;
}

void PeripheralSettingsManager::reset() {
    // Clear NVS
    preferences.clear();

    // Reset to defaults
    settings.uart1Mode = PeripheralDefaults::UART1_MODE;
    settings.uart1Baud = PeripheralDefaults::UART1_BAUD;
    settings.uart1PwmFreq = PeripheralDefaults::UART1_PWM_FREQ;
    settings.uart1PwmDuty = PeripheralDefaults::UART1_PWM_DUTY;
    settings.uart1PwmEnabled = PeripheralDefaults::UART1_PWM_ENABLED;

    settings.uart2Baud = PeripheralDefaults::UART2_BAUD;

    settings.buzzerFreq = PeripheralDefaults::BUZZER_FREQ;
    settings.buzzerDuty = PeripheralDefaults::BUZZER_DUTY;
    settings.buzzerEnabled = PeripheralDefaults::BUZZER_ENABLED;

    settings.ledPwmFreq = PeripheralDefaults::LED_PWM_FREQ;
    settings.ledBrightness = PeripheralDefaults::LED_BRIGHTNESS;
    settings.ledEnabled = PeripheralDefaults::LED_ENABLED;

    settings.relayState = PeripheralDefaults::RELAY_STATE;
    settings.gpioState = PeripheralDefaults::GPIO_STATE;

    settings.keyControlAdjustDuty = PeripheralDefaults::KEY_ADJUST_DUTY;
    settings.keyDutyStep = PeripheralDefaults::KEY_DUTY_STEP;
    settings.keyFreqStep = PeripheralDefaults::KEY_FREQ_STEP;
    settings.keyControlEnabled = PeripheralDefaults::KEY_CONTROL_ENABLED;

    Serial.println("[PeripheralSettings] Settings reset to defaults");
}

PeripheralSettings& PeripheralSettingsManager::get() {
    return settings;
}

const PeripheralSettings& PeripheralSettingsManager::get() const {
    return settings;
}
