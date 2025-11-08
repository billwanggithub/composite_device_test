#ifndef PERIPHERAL_SETTINGS_H
#define PERIPHERAL_SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * @brief Peripheral configuration settings structure
 *
 * Stores all peripheral parameters that can be saved to NVS
 */
struct PeripheralSettings {
    // UART1 Multiplexer Settings
    // NOTE: uart1Mode is saved to NVS but NOT applied at startup
    // UART1 always defaults to PWM/RPM mode and can only be changed via commands
    uint8_t uart1Mode = 0;              // 0=Disabled, 1=UART, 2=PWM_RPM (saved but not applied)
    uint32_t uart1Baud = 115200;        // UART mode baud rate (2400-1500000)
    uint32_t uart1PwmFreq = 1000;       // PWM mode frequency (1-500000 Hz)
    float uart1PwmDuty = 50.0;          // PWM mode duty cycle (0-100%)
    bool uart1PwmEnabled = false;       // PWM mode enable state

    // UART2 Settings
    uint32_t uart2Baud = 115200;        // Baud rate (2400-1500000)

    // Buzzer Settings
    uint32_t buzzerFreq = 2000;         // Frequency (10-20000 Hz)
    float buzzerDuty = 50.0;            // Duty cycle (0-100%)
    bool buzzerEnabled = false;         // Enable state

    // LED PWM Settings
    uint32_t ledPwmFreq = 1000;         // Frequency (100-20000 Hz)
    float ledBrightness = 50.0;         // Brightness (0-100%)
    bool ledEnabled = false;            // Enable state

    // Relay Settings
    bool relayState = false;            // Initial state (false=OFF, true=ON)

    // GPIO Settings
    bool gpioState = false;             // Initial state (false=LOW, true=HIGH)

    // User Keys Settings
    bool keyControlAdjustDuty = true;   // Control mode (true=duty, false=frequency)
    float keyDutyStep = 1.0;            // Duty step size (0.1-10%)
    uint32_t keyFreqStep = 100;         // Frequency step size (10-10000 Hz)
    bool keyControlEnabled = true;      // Key control enabled
};

/**
 * @brief Peripheral settings manager with NVS persistence
 *
 * Handles loading, saving, and resetting peripheral configuration settings
 * to/from ESP32 Non-Volatile Storage (NVS).
 *
 * Usage:
 *   PeripheralSettingsManager settingsManager;
 *   settingsManager.begin();
 *   settingsManager.load();
 *   settingsManager.get().buzzerFreq = 3000;
 *   settingsManager.save();
 */
class PeripheralSettingsManager {
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
     * @return Reference to PeripheralSettings structure
     */
    PeripheralSettings& get();

    /**
     * @brief Get const reference to current settings
     * @return Const reference to PeripheralSettings structure
     */
    const PeripheralSettings& get() const;

private:
    PeripheralSettings settings;
    Preferences preferences;
    bool initialized = false;

    // NVS namespace
    static const char* NVS_NAMESPACE;

    // NVS keys
    static const char* KEY_UART1_MODE;
    static const char* KEY_UART1_BAUD;
    static const char* KEY_UART1_PWM_FREQ;
    static const char* KEY_UART1_PWM_DUTY;
    static const char* KEY_UART1_PWM_EN;
    static const char* KEY_UART2_BAUD;
    static const char* KEY_BUZZER_FREQ;
    static const char* KEY_BUZZER_DUTY;
    static const char* KEY_BUZZER_EN;
    static const char* KEY_LED_FREQ;
    static const char* KEY_LED_BRIGHT;
    static const char* KEY_LED_EN;
    static const char* KEY_RELAY_STATE;
    static const char* KEY_GPIO_STATE;
    static const char* KEY_KEY_MODE;
    static const char* KEY_KEY_DUTY_STEP;
    static const char* KEY_KEY_FREQ_STEP;
    static const char* KEY_KEY_CTRL_EN;
};

// Default configuration values
namespace PeripheralDefaults {
    constexpr uint8_t UART1_MODE = 0;  // Disabled
    constexpr uint32_t UART1_BAUD = 115200;
    constexpr uint32_t UART1_PWM_FREQ = 1000;
    constexpr float UART1_PWM_DUTY = 50.0;
    constexpr bool UART1_PWM_ENABLED = false;

    constexpr uint32_t UART2_BAUD = 115200;

    constexpr uint32_t BUZZER_FREQ = 2000;
    constexpr float BUZZER_DUTY = 50.0;
    constexpr bool BUZZER_ENABLED = false;

    constexpr uint32_t LED_PWM_FREQ = 1000;
    constexpr float LED_BRIGHTNESS = 50.0;
    constexpr bool LED_ENABLED = false;

    constexpr bool RELAY_STATE = false;
    constexpr bool GPIO_STATE = false;

    constexpr bool KEY_ADJUST_DUTY = true;
    constexpr float KEY_DUTY_STEP = 1.0;
    constexpr uint32_t KEY_FREQ_STEP = 100;
    constexpr bool KEY_CONTROL_ENABLED = true;
}

// Validation limits
namespace PeripheralLimits {
    constexpr uint32_t UART_MIN_BAUD = 2400;
    constexpr uint32_t UART_MAX_BAUD = 1500000;

    constexpr uint32_t UART1_PWM_MIN_FREQ = 1;
    constexpr uint32_t UART1_PWM_MAX_FREQ = 500000;

    constexpr uint32_t BUZZER_MIN_FREQ = 10;
    constexpr uint32_t BUZZER_MAX_FREQ = 20000;

    constexpr uint32_t LED_MIN_FREQ = 100;
    constexpr uint32_t LED_MAX_FREQ = 20000;

    constexpr float MIN_DUTY = 0.0;
    constexpr float MAX_DUTY = 100.0;

    constexpr float KEY_MIN_DUTY_STEP = 0.1;
    constexpr float KEY_MAX_DUTY_STEP = 10.0;
    constexpr uint32_t KEY_MIN_FREQ_STEP = 10;
    constexpr uint32_t KEY_MAX_FREQ_STEP = 10000;
}

#endif // PERIPHERAL_SETTINGS_H
