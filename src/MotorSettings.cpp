#include "MotorSettings.h"

// NVS namespace and keys
const char* MotorSettingsManager::NVS_NAMESPACE = "motor_cfg";
const char* MotorSettingsManager::KEY_FREQUENCY = "frequency";
const char* MotorSettingsManager::KEY_DUTY = "duty";
const char* MotorSettingsManager::KEY_POLE_PAIRS = "polePairs";
const char* MotorSettingsManager::KEY_MAX_FREQUENCY = "maxFreq";
const char* MotorSettingsManager::KEY_MAX_SAFE_RPM = "maxRPM";
const char* MotorSettingsManager::KEY_LED_BRIGHTNESS = "ledBright";
const char* MotorSettingsManager::KEY_RPM_UPDATE_RATE = "rpmRate";

bool MotorSettingsManager::begin() {
    if (initialized) {
        return true;
    }

    // Open NVS in read-write mode
    bool success = preferences.begin(NVS_NAMESPACE, false);
    if (success) {
        initialized = true;
    }

    return success;
}

bool MotorSettingsManager::load() {
    if (!initialized) {
        return false;
    }

    // Load settings from NVS with defaults
    settings.frequency = preferences.getUInt(KEY_FREQUENCY, MotorDefaults::FREQUENCY);
    settings.duty = preferences.getFloat(KEY_DUTY, MotorDefaults::DUTY);
    settings.polePairs = preferences.getUChar(KEY_POLE_PAIRS, MotorDefaults::POLE_PAIRS);
    settings.maxFrequency = preferences.getUInt(KEY_MAX_FREQUENCY, MotorDefaults::MAX_FREQUENCY);
    settings.maxSafeRPM = preferences.getUInt(KEY_MAX_SAFE_RPM, MotorDefaults::MAX_SAFE_RPM);
    settings.ledBrightness = preferences.getUChar(KEY_LED_BRIGHTNESS, MotorDefaults::LED_BRIGHTNESS);
    settings.rpmUpdateRate = preferences.getUInt(KEY_RPM_UPDATE_RATE, MotorDefaults::RPM_UPDATE_RATE);

    // Validate loaded values and clamp to safe ranges
    if (settings.frequency < MotorLimits::MIN_FREQUENCY ||
        settings.frequency > MotorLimits::MAX_FREQUENCY) {
        settings.frequency = MotorDefaults::FREQUENCY;
    }

    if (settings.duty < MotorLimits::MIN_DUTY ||
        settings.duty > MotorLimits::MAX_DUTY) {
        settings.duty = MotorDefaults::DUTY;
    }

    if (settings.polePairs < MotorLimits::MIN_POLE_PAIRS ||
        settings.polePairs > MotorLimits::MAX_POLE_PAIRS) {
        settings.polePairs = MotorDefaults::POLE_PAIRS;
    }

    if (settings.maxFrequency < MotorLimits::MIN_FREQUENCY ||
        settings.maxFrequency > MotorLimits::MAX_FREQUENCY) {
        settings.maxFrequency = MotorDefaults::MAX_FREQUENCY;
    }

    if (settings.rpmUpdateRate < MotorLimits::MIN_RPM_UPDATE_RATE ||
        settings.rpmUpdateRate > MotorLimits::MAX_RPM_UPDATE_RATE) {
        settings.rpmUpdateRate = MotorDefaults::RPM_UPDATE_RATE;
    }

    return true;
}

bool MotorSettingsManager::save() {
    if (!initialized) {
        return false;
    }

    // Save all settings to NVS
    preferences.putUInt(KEY_FREQUENCY, settings.frequency);
    preferences.putFloat(KEY_DUTY, settings.duty);
    preferences.putUChar(KEY_POLE_PAIRS, settings.polePairs);
    preferences.putUInt(KEY_MAX_FREQUENCY, settings.maxFrequency);
    preferences.putUInt(KEY_MAX_SAFE_RPM, settings.maxSafeRPM);
    preferences.putUChar(KEY_LED_BRIGHTNESS, settings.ledBrightness);
    preferences.putUInt(KEY_RPM_UPDATE_RATE, settings.rpmUpdateRate);

    return true;
}

void MotorSettingsManager::reset() {
    // Reset to factory defaults
    settings.frequency = MotorDefaults::FREQUENCY;
    settings.duty = MotorDefaults::DUTY;
    settings.polePairs = MotorDefaults::POLE_PAIRS;
    settings.maxFrequency = MotorDefaults::MAX_FREQUENCY;
    settings.maxSafeRPM = MotorDefaults::MAX_SAFE_RPM;
    settings.ledBrightness = MotorDefaults::LED_BRIGHTNESS;
    settings.rpmUpdateRate = MotorDefaults::RPM_UPDATE_RATE;

    // Clear NVS storage
    if (initialized) {
        preferences.clear();
    }
}

MotorSettings& MotorSettingsManager::get() {
    return settings;
}

const MotorSettings& MotorSettingsManager::get() const {
    return settings;
}
