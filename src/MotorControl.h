#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
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
     * @brief Set motor pole pairs
     * @param pairs Number of pole pairs (1-12)
     * @return true if pole pairs set successfully
     *
     * Updates the pole pair count used for RPM calculation.
     * The pole pair count affects how tachometer frequency is converted to RPM.
     */
    bool setPolePairs(uint8_t pairs);

    /**
     * @brief Get current motor pole pairs
     * @return Current pole pair count
     */
    uint8_t getPolePairs() const;

    /**
     * @brief Update RPM reading
     *
     * Should be called periodically (e.g., every 100ms) from motor task.
     * Reads tachometer capture data and calculates RPM.
     * Applies moving average filtering if enabled.
     */
    void updateRPM();

    /**
     * @brief Update PWM ramping (if active)
     *
     * Should be called periodically (e.g., every 10-20ms) from motor task.
     * Gradually transitions PWM frequency and duty to target values.
     */
    void updateRamping();

    /**
     * @brief Get current RPM reading (filtered)
     * @return Filtered RPM value
     */
    float getCurrentRPM() const;

    /**
     * @brief Get raw RPM reading (unfiltered)
     * @return Raw RPM value from last capture
     */
    float getRawRPM() const;

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
     * @brief Check if emergency stop is active
     * @return true if emergency stop has been triggered
     */
    bool isEmergencyStopActive() const;

    /**
     * @brief Clear emergency stop flag
     *
     * Clears the emergency stop condition after the issue has been resolved.
     * User must explicitly clear the emergency stop to resume normal operation.
     */
    void clearEmergencyStop();

    /**
     * @brief Get the RPM value at the time emergency stop was triggered
     * @return RPM value when emergency stop activated (0 if not triggered)
     */
    float getEmergencyStopTriggerRPM() const;

    /**
     * @brief Set PWM frequency with ramping
     * @param frequency Target frequency in Hz
     * @param rampTimeMs Ramp duration in milliseconds (0 = immediate)
     * @return true if ramp started successfully
     */
    bool setPWMFrequencyRamped(uint32_t frequency, uint32_t rampTimeMs);

    /**
     * @brief Set PWM duty with ramping
     * @param duty Target duty cycle in percent (0.0 - 100.0)
     * @param rampTimeMs Ramp duration in milliseconds (0 = immediate)
     * @return true if ramp started successfully
     */
    bool setPWMDutyRamped(float duty, uint32_t rampTimeMs);

    /**
     * @brief Check if ramping is active
     * @return true if frequency or duty ramping is in progress
     */
    bool isRamping() const;

    /**
     * @brief Configure RPM filter
     * @param windowSize Number of samples for moving average (1-20)
     */
    void setRPMFilterSize(uint8_t windowSize);

    /**
     * @brief Get current RPM filter window size
     * @return Filter window size
     */
    uint8_t getRPMFilterSize() const;

    /**
     * @brief Feed the watchdog timer
     *
     * Must be called periodically from motor task to prevent watchdog reset.
     * If not called within timeout period, system will reset.
     */
    void feedWatchdog();

    /**
     * @brief Check watchdog timeout
     * @return true if watchdog is OK, false if timeout occurred
     */
    bool checkWatchdog();

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
    float currentRPM = 0.0;          // Filtered RPM
    float rawRPM = 0.0;              // Raw unfiltered RPM
    float currentInputFrequency = 0.0;

    // Capture data (shared with ISR)
    static volatile uint32_t capturePeriod;
    static volatile bool newCaptureAvailable;
    static volatile uint32_t lastCaptureTime;

    // Safety monitoring
    bool emergencyStopActive = false;
    float emergencyStopTriggerRPM = 0.0;  // RPM at the time emergency stop was triggered
    unsigned long lastRPMUpdateTime = 0;
    float lastRPM = 0.0;

    // RPM Filtering
    static const uint8_t MAX_FILTER_SIZE = 20;
    float rpmFilterBuffer[MAX_FILTER_SIZE];
    uint8_t rpmFilterSize = 5;          // Default 5-sample moving average
    uint8_t rpmFilterIndex = 0;
    uint8_t rpmFilterCount = 0;
    bool rpmFilterEnabled = true;

    // PWM Ramping
    bool frequencyRampActive = false;
    bool dutyRampActive = false;
    uint32_t targetFrequency = 0;
    float targetDuty = 0.0;
    uint32_t frequencyRampStart = 0;
    uint32_t frequencyRampDuration = 0;
    uint32_t frequencyStartValue = 0;
    unsigned long dutyRampStart = 0;
    uint32_t dutyRampDuration = 0;
    float dutyStartValue = 0.0;

    // Watchdog Timer
    bool watchdogEnabled = false;
    unsigned long lastWatchdogFeed = 0;
    static const uint32_t WATCHDOG_TIMEOUT_MS = 5000;  // 5 second timeout

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

    /**
     * @brief Apply RPM filter to raw value
     * @param rawValue Raw RPM reading
     * @return Filtered RPM value
     */
    float applyRPMFilter(float rawValue);
};

#endif // MOTOR_CONTROL_H
