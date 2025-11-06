#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include "driver/mcpwm.h"
#include "MotorSettings.h"

/**
 * @brief Motor control class using ESP32-S3 MCPWM peripheral
 *
 * This class provides:
 * - High-precision PWM motor control (MCPWM Unit 1)
 * - Hardware-based tachometer input (MCPWM Unit 0 Capture)
 * - RPM calculation from tachometer frequency
 * - Safety monitoring (overspeed, stall detection)
 * - Pulse output for change notification
 *
 * Hardware Configuration:
 * - PWM Output: GPIO 10 (MCPWM1_A)
 * - Tachometer Input: GPIO 11 (MCPWM0_CAP0)
 * - Pulse Output: GPIO 12 (change notification)
 *
 * Usage:
 *   MotorControl motor;
 *   motor.begin(&settingsManager.get());
 *   motor.setPWMFrequency(15000);  // 15 kHz
 *   motor.setPWMDuty(75.0);        // 75%
 *   motor.updateRPM();             // Call periodically
 *   float rpm = motor.getCurrentRPM();
 */
class MotorControl {
public:
    /**
     * @brief Constructor
     */
    MotorControl();

    /**
     * @brief Initialize motor control system
     * @param settings Pointer to motor settings structure
     * @return true if initialization successful
     *
     * Initializes:
     * - MCPWM PWM output on GPIO 10
     * - MCPWM Capture tachometer input on GPIO 11
     * - Pulse output on GPIO 12
     */
    bool begin(MotorSettings* settings);

    /**
     * @brief Shutdown motor control system
     *
     * Stops PWM output and disables capture
     */
    void end();

    /**
     * @brief Set PWM frequency
     * @param frequency Frequency in Hz (10 - 500,000)
     * @return true if frequency set successfully
     *
     * Uses MCPWM for glitch-free frequency changes.
     * Change detection prevents unnecessary updates.
     */
    bool setPWMFrequency(uint32_t frequency);

    /**
     * @brief Set PWM duty cycle
     * @param duty Duty cycle in percent (0.0 - 100.0)
     * @return true if duty set successfully
     *
     * Change detection prevents unnecessary updates.
     */
    bool setPWMDuty(float duty);

    /**
     * @brief Get current PWM frequency
     * @return Current frequency in Hz
     */
    uint32_t getPWMFrequency() const;

    /**
     * @brief Get current PWM duty cycle
     * @return Current duty cycle in percent
     */
    float getPWMDuty() const;

    /**
     * @brief Update RPM reading
     *
     * Should be called periodically (e.g., every 100ms) from motor task.
     * Reads tachometer capture data and calculates RPM.
     */
    void updateRPM();

    /**
     * @brief Get current RPM reading
     * @return RPM value
     */
    float getCurrentRPM() const;

    /**
     * @brief Get current tachometer input frequency
     * @return Frequency in Hz
     */
    float getInputFrequency() const;

    /**
     * @brief Check if motor control is initialized
     * @return true if initialized
     */
    bool isInitialized() const;

    /**
     * @brief Check if tachometer capture is initialized
     * @return true if capture initialized
     */
    bool isCaptureInitialized() const;

    /**
     * @brief Perform safety checks
     * @return true if all safety checks pass
     *
     * Checks for:
     * - Overspeed condition
     * - Stall condition (motor not responding to PWM)
     * - Unexpected RPM changes
     *
     * If check fails, emergency stop is triggered automatically.
     */
    bool checkSafety();

    /**
     * @brief Emergency stop - immediately set duty to 0%
     */
    void emergencyStop();

    /**
     * @brief Send pulse on GPIO 12 (change notification)
     *
     * Sends a brief pulse when PWM parameters change.
     * Useful for triggering external systems or indicators.
     */
    void sendPulse();

    /**
     * @brief Get uptime in milliseconds
     * @return Uptime since begin() call
     */
    unsigned long getUptime() const;

private:
    // Settings reference
    MotorSettings* pSettings = nullptr;

    // Initialization flags
    bool mcpwmInitialized = false;
    bool captureInitialized = false;
    unsigned long initTime = 0;

    // Current state
    uint32_t currentFrequency = 0;
    float currentDuty = 0.0;
    float currentRPM = 0.0;
    float currentInputFrequency = 0.0;

    // Capture data (shared with ISR)
    static volatile uint32_t capturePeriod;
    static volatile bool newCaptureAvailable;
    static volatile uint32_t lastCaptureTime;

    // Safety monitoring
    bool emergencyStopActive = false;
    unsigned long lastRPMUpdateTime = 0;
    float lastRPM = 0.0;

    // Hardware configuration
    static const int PWM_OUTPUT_PIN = 10;
    static const int TACHOMETER_INPUT_PIN = 11;
    static const int PULSE_OUTPUT_PIN = 12;

    // MCPWM configuration for PWM output
    static const mcpwm_unit_t PWM_MCPWM_UNIT = MCPWM_UNIT_1;
    static const mcpwm_timer_t PWM_MCPWM_TIMER = MCPWM_TIMER_0;
    static const mcpwm_io_signals_t PWM_MCPWM_GEN = MCPWM0A;

    // MCPWM configuration for tachometer capture
    static const mcpwm_unit_t CAP_MCPWM_UNIT = MCPWM_UNIT_0;
    static const mcpwm_capture_signal_t CAP_SIGNAL = MCPWM_SELECT_CAP0;

    // Timer clock frequency (80 MHz)
    static const uint32_t MCPWM_CAP_TIMER_CLK = 80000000;

    // Pulse width for change notification (microseconds)
    static const uint32_t PULSE_WIDTH_US = 10;

    /**
     * @brief Initialize PWM output
     * @return true if successful
     */
    bool initPWM();

    /**
     * @brief Initialize tachometer capture
     * @return true if successful
     */
    bool initCapture();

    /**
     * @brief MCPWM Capture ISR callback
     *
     * Runs in ISR context - must be fast!
     * Captures timestamp and calculates period.
     */
    static bool IRAM_ATTR captureCallback(mcpwm_unit_t mcpwm,
                                          mcpwm_capture_channel_id_t cap_channel,
                                          const cap_event_data_t *edata,
                                          void *user_data);
};

#endif // MOTOR_CONTROL_H
