#ifndef BUZZER_CONTROL_H
#define BUZZER_CONTROL_H

#include <Arduino.h>
#include "driver/ledc.h"
#include "PeripheralPins.h"

/**
 * @brief Buzzer Control using LEDC PWM
 *
 * Controls a passive buzzer with PWM frequency and duty cycle:
 * - Frequency range: 10 Hz - 20 kHz
 * - Duty cycle: 0% - 100% (typically 50% for max volume)
 * - ON/OFF control
 * - Configurable via web interface
 *
 * Hardware:
 * - GPIO 13: Buzzer PWM output (LEDC Channel 0)
 * - Passive buzzer or active buzzer module
 *
 * Usage:
 *   BuzzerControl buzzer;
 *   buzzer.begin();
 *   buzzer.setFrequency(2000);  // 2 kHz
 *   buzzer.setDuty(50.0);       // 50% duty
 *   buzzer.enable(true);        // Turn on
 */
class BuzzerControl {
public:
    /**
     * @brief Constructor
     */
    BuzzerControl();

    /**
     * @brief Initialize buzzer control
     * @param frequency Initial frequency in Hz (default: 2000 Hz)
     * @param duty Initial duty cycle in percent (default: 50%)
     * @return true if initialization successful
     */
    bool begin(uint32_t frequency = 2000, float duty = 50.0);

    /**
     * @brief Set buzzer frequency
     * @param frequency Frequency in Hz (10 - 20,000)
     * @return true if successful
     */
    bool setFrequency(uint32_t frequency);

    /**
     * @brief Set buzzer duty cycle
     * @param duty Duty cycle in percent (0.0 - 100.0)
     * @return true if successful
     */
    bool setDuty(float duty);

    /**
     * @brief Enable/disable buzzer output
     * @param enabled true to enable, false to disable
     */
    void enable(bool enabled);

    /**
     * @brief Check if buzzer is enabled
     * @return true if enabled
     */
    bool isEnabled() const { return buzzerEnabled; }

    /**
     * @brief Get current frequency
     * @return Current frequency in Hz
     */
    uint32_t getFrequency() const { return currentFrequency; }

    /**
     * @brief Get current duty cycle
     * @return Current duty cycle in percent
     */
    float getDuty() const { return currentDuty; }

    /**
     * @brief Play a beep (fixed duration)
     * @param frequency Beep frequency in Hz
     * @param durationMs Beep duration in milliseconds
     * @param duty Beep duty cycle (default: 50%)
     */
    void beep(uint32_t frequency, uint32_t durationMs, float duty = 50.0);

    /**
     * @brief Play a melody (sequence of tones)
     * @param frequencies Array of frequencies in Hz (0 = rest)
     * @param durations Array of durations in milliseconds
     * @param count Number of tones in sequence
     * @param duty Duty cycle for all tones (default: 50%)
     */
    void playMelody(const uint32_t* frequencies, const uint32_t* durations,
                    uint8_t count, float duty = 50.0);

    /**
     * @brief Stop any ongoing beep or melody
     */
    void stop();

    /**
     * @brief Check if buzzer is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return initialized; }

private:
    bool initialized = false;
    bool buzzerEnabled = false;
    uint32_t currentFrequency = 2000;  // Default 2 kHz
    float currentDuty = 50.0;          // Default 50%

    /**
     * @brief Validate frequency range
     * @param frequency Frequency to validate
     * @return true if valid
     */
    bool validateFrequency(uint32_t frequency);

    /**
     * @brief Validate duty cycle range
     * @param duty Duty cycle to validate
     * @return true if valid
     */
    bool validateDuty(float duty);
};

#endif // BUZZER_CONTROL_H
