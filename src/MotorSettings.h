#ifndef MOTOR_SETTINGS_H
#define MOTOR_SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * @brief Motor configuration settings structure
 *
 * Stores all motor control parameters that can be saved to NVS
 */
struct MotorSettings {
    uint32_t frequency = 10000;        // PWM frequency in Hz (10-500000)
    float duty = 0.0;                  // PWM duty cycle in percent (0.0-100.0)
    uint8_t polePairs = 2;             // Motor pole pairs (1-12)
    uint32_t maxFrequency = 500000;    // Maximum frequency limit (MCPWM driver limit: 500 kHz)
    uint32_t maxSafeRPM = 500000;      // Overspeed protection threshold
    uint8_t ledBrightness = 25;        // RGB LED brightness (0-255)
    uint32_t rpmUpdateRate = 100;      // RPM update interval in ms (20-1000)
    char language[10] = "en";          // UI language (en, zh-CN, zh-TW)
};

/**
 * @brief Motor settings manager with NVS persistence
 *
 * Handles loading, saving, and resetting motor configuration settings
 * to/from ESP32 Non-Volatile Storage (NVS).
 *
 * Usage:
 *   MotorSettingsManager settingsManager;
 *   settingsManager.begin();
 *   settingsManager.load();
 *   settingsManager.get().frequency = 15000;
 *   settingsManager.save();
 */
class MotorSettingsManager {
public:
    /**
     * @brief Initialize the settings manager
     * @return true if initialization successful
     */
    bool begin();

    /**
     * @brief Load settings from NVS
     * @return true if settings loaded successfully
     *
     * If NVS is empty or corrupt, default values are used
     */
    bool load();

    /**
     * @brief Save current settings to NVS
     * @return true if settings saved successfully
     */
    bool save();

    /**
     * @brief Reset settings to factory defaults
     *
     * Resets in-memory settings and clears NVS storage
     */
    void reset();

    /**
     * @brief Get reference to current settings
     * @return Reference to MotorSettings structure
     */
    MotorSettings& get();

    /**
     * @brief Get const reference to current settings
     * @return Const reference to MotorSettings structure
     */
    const MotorSettings& get() const;

private:
    MotorSettings settings;
    Preferences preferences;
    bool initialized = false;

    // NVS namespace
    static const char* NVS_NAMESPACE;

    // NVS keys
    static const char* KEY_FREQUENCY;
    static const char* KEY_DUTY;
    static const char* KEY_POLE_PAIRS;
    static const char* KEY_MAX_FREQUENCY;
    static const char* KEY_MAX_SAFE_RPM;
    static const char* KEY_LED_BRIGHTNESS;
    static const char* KEY_RPM_UPDATE_RATE;
    static const char* KEY_LANGUAGE;
};

// Default configuration values
namespace MotorDefaults {
    constexpr uint32_t FREQUENCY = 10000;
    constexpr float DUTY = 0.0;
    constexpr uint8_t POLE_PAIRS = 2;
    constexpr uint32_t MAX_FREQUENCY = 500000;  // 500 kHz - MCPWM driver limit
    constexpr uint32_t MAX_SAFE_RPM = 500000;
    constexpr uint8_t LED_BRIGHTNESS = 25;
    constexpr uint32_t RPM_UPDATE_RATE = 100;
}

// Validation limits
namespace MotorLimits {
    constexpr uint32_t MIN_FREQUENCY = 10;
    constexpr uint32_t MAX_FREQUENCY = 500000;  // 500 kHz - verified MCPWM limit
    constexpr float MIN_DUTY = 0.0;
    constexpr float MAX_DUTY = 100.0;
    constexpr uint8_t MIN_POLE_PAIRS = 1;
    constexpr uint8_t MAX_POLE_PAIRS = 12;
    constexpr uint32_t MIN_RPM_UPDATE_RATE = 20;
    constexpr uint32_t MAX_RPM_UPDATE_RATE = 1000;
}

#endif // MOTOR_SETTINGS_H
