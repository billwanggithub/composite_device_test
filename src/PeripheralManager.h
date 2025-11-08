#ifndef PERIPHERAL_MANAGER_H
#define PERIPHERAL_MANAGER_H

#include <Arduino.h>
#include "UART1Mux.h"
#include "UART2Manager.h"
#include "UserKeys.h"
#include "BuzzerControl.h"
#include "LEDPWMControl.h"
#include "RelayControl.h"
#include "GPIOControl.h"
#include "MotorControl.h"
#include "PeripheralSettings.h"

/**
 * @brief Peripheral Manager - Centralized peripheral control
 *
 * Manages all peripherals in the system:
 * - UART1 (multiplexable between UART and PWM/RPM)
 * - UART2 (standard UART)
 * - User Keys (3 buttons with debouncing)
 * - Buzzer PWM control
 * - LED PWM control
 * - Relay control
 * - General GPIO output
 *
 * Provides:
 * - Unified initialization
 * - Periodic update function
 * - Access to all peripheral instances
 * - Key event handling for motor duty adjustment
 *
 * Usage:
 *   PeripheralManager peripherals;
 *   peripherals.begin(&motorControl);
 *
 *   // In main loop or task:
 *   peripherals.update();
 */
class PeripheralManager {
public:
    /**
     * @brief Constructor
     */
    PeripheralManager();

    /**
     * @brief Initialize all peripherals
     * @param motor Pointer to motor control instance (for key control)
     * @return true if all peripherals initialized successfully
     */
    bool begin(MotorControl* motor = nullptr);

    /**
     * @brief Update all peripherals
     *
     * Should be called periodically (e.g., every 20-50ms) from peripheral task.
     * Updates:
     * - User keys (debouncing, event detection)
     * - UART1 RPM measurement (if in PWM/RPM mode)
     * - Key-based motor duty adjustment
     */
    void update();

    // ========================================================================
    // Peripheral Access
    // ========================================================================

    UART1Mux& getUART1() { return uart1; }
    UART2Manager& getUART2() { return uart2; }
    UserKeys& getKeys() { return keys; }
    BuzzerControl& getBuzzer() { return buzzer; }
    LEDPWMControl& getLEDPWM() { return ledPWM; }
    RelayControl& getRelay() { return relay; }
    GPIOControl& getGPIO() { return gpioOut; }

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Set duty/frequency step sizes for key control
     * @param dutyStep Duty step in percent (default: 1.0%)
     * @param frequencyStep Frequency step in Hz (default: 100 Hz)
     */
    void setStepSizes(float dutyStep, uint32_t frequencyStep);

    /**
     * @brief Get duty step size
     * @return Duty step in percent
     */
    float getDutyStep() const { return dutyStepSize; }

    /**
     * @brief Get frequency step size
     * @return Frequency step in Hz
     */
    uint32_t getFrequencyStep() const { return frequencyStepSize; }

    /**
     * @brief Enable/disable key control of motor
     * @param enabled true to enable, false to disable
     */
    void setKeyControlEnabled(bool enabled) { keyControlEnabled = enabled; }

    /**
     * @brief Check if key control is enabled
     * @return true if enabled
     */
    bool isKeyControlEnabled() const { return keyControlEnabled; }

    /**
     * @brief Set key control mode (duty or frequency)
     * @param adjustDuty true = adjust duty, false = adjust frequency
     */
    void setKeyControlMode(bool adjustDuty) { keyControlAdjustsDuty = adjustDuty; }

    /**
     * @brief Check if key control adjusts duty (vs frequency)
     * @return true if adjusting duty, false if adjusting frequency
     */
    bool isKeyControlAdjustingDuty() const { return keyControlAdjustsDuty; }

    /**
     * @brief Get statistics string for all peripherals
     * @return Statistics string
     */
    String getStatistics();

    /**
     * @brief Check if all peripherals are initialized
     * @return true if all initialized
     */
    bool isInitialized() const { return allInitialized; }

    // ========================================================================
    // Settings Management
    // ========================================================================

    /**
     * @brief Initialize settings manager
     * @return true if successful
     */
    bool beginSettings();

    /**
     * @brief Load settings from NVS
     * @return true if successful
     */
    bool loadSettings();

    /**
     * @brief Save current peripheral states to NVS
     * @return true if successful
     */
    bool saveSettings();

    /**
     * @brief Apply loaded settings to all peripherals
     * @return true if successful
     */
    bool applySettings();

    /**
     * @brief Reset all settings to factory defaults
     */
    void resetSettings();

    /**
     * @brief Get reference to settings manager
     * @return Reference to settings manager
     */
    PeripheralSettingsManager& getSettingsManager() { return settingsManager; }

private:
    // Peripheral instances
    UART1Mux uart1;
    UART2Manager uart2;
    UserKeys keys;
    BuzzerControl buzzer;
    LEDPWMControl ledPWM;
    RelayControl relay;
    GPIOControl gpioOut;

    // Motor control reference (for key control)
    MotorControl* pMotorControl = nullptr;

    // Settings manager
    PeripheralSettingsManager settingsManager;

    // Initialization state
    bool allInitialized = false;

    // Key control configuration
    bool keyControlEnabled = true;
    bool keyControlAdjustsDuty = true;  // true = duty, false = frequency
    float dutyStepSize = 1.0;          // Default 1% duty step
    uint32_t frequencyStepSize = 100;  // Default 100 Hz frequency step

    /**
     * @brief Handle user key events
     *
     * Processes key events and adjusts motor parameters accordingly.
     */
    void handleKeyEvents();

    /**
     * @brief Adjust motor duty (increase or decrease)
     * @param increase true to increase, false to decrease
     */
    void adjustMotorDuty(bool increase);

    /**
     * @brief Adjust motor frequency (increase or decrease)
     * @param increase true to increase, false to decrease
     */
    void adjustMotorFrequency(bool increase);
};

#endif // PERIPHERAL_MANAGER_H
