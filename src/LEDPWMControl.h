#ifndef LED_PWM_CONTROL_H
#define LED_PWM_CONTROL_H

#include <Arduino.h>
#include "driver/ledc.h"
#include "PeripheralPins.h"

/**
 * @brief LED PWM Control using LEDC
 *
 * Controls LED brightness with PWM:
 * - Frequency range: 100 Hz - 20 kHz (flicker-free)
 * - Duty cycle: 0% - 100% for brightness control
 * - ON/OFF control
 * - Configurable via web interface
 *
 * Hardware:
 * - GPIO 14: LED PWM output (LEDC Channel 1)
 * - LED or LED strip (with appropriate current limiting)
 *
 * Usage:
 *   LEDPWMControl led;
 *   led.begin();
 *   led.setFrequency(1000);  // 1 kHz
 *   led.setBrightness(75.0); // 75% brightness
 *   led.enable(true);        // Turn on
 */
class LEDPWMControl {
public:
    /**
     * @brief Constructor
     */
    LEDPWMControl();

    /**
     * @brief Initialize LED PWM control
     * @param frequency Initial frequency in Hz (default: 1000 Hz)
     * @param brightness Initial brightness in percent (default: 50%)
     * @return true if initialization successful
     */
    bool begin(uint32_t frequency = 1000, float brightness = 50.0);

    /**
     * @brief Set LED PWM frequency
     * @param frequency Frequency in Hz (100 - 20,000)
     * @return true if successful
     */
    bool setFrequency(uint32_t frequency);

    /**
     * @brief Set LED brightness (duty cycle)
     * @param brightness Brightness in percent (0.0 - 100.0)
     * @return true if successful
     */
    bool setBrightness(float brightness);

    /**
     * @brief Enable/disable LED output
     * @param enabled true to enable, false to disable
     */
    void enable(bool enabled);

    /**
     * @brief Check if LED is enabled
     * @return true if enabled
     */
    bool isEnabled() const { return ledEnabled; }

    /**
     * @brief Get current frequency
     * @return Current frequency in Hz
     */
    uint32_t getFrequency() const { return currentFrequency; }

    /**
     * @brief Get current brightness
     * @return Current brightness in percent
     */
    float getBrightness() const { return currentBrightness; }

    /**
     * @brief Fade to target brightness
     * @param targetBrightness Target brightness in percent
     * @param fadeTimeMs Fade duration in milliseconds
     * @param steps Number of fade steps (more steps = smoother)
     */
    void fadeTo(float targetBrightness, uint32_t fadeTimeMs, uint16_t steps = 50);

    /**
     * @brief Blink LED
     * @param onTimeMs On time in milliseconds
     * @param offTimeMs Off time in milliseconds
     * @param cycles Number of blink cycles (0 = infinite)
     *
     * Note: This is a blocking function. Use with care.
     */
    void blink(uint32_t onTimeMs, uint32_t offTimeMs, uint16_t cycles = 1);

    /**
     * @brief Stop any ongoing fade or blink
     */
    void stop();

    /**
     * @brief Check if LED is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return initialized; }

private:
    bool initialized = false;
    bool ledEnabled = false;
    uint32_t currentFrequency = 1000;  // Default 1 kHz
    float currentBrightness = 50.0;    // Default 50%

    /**
     * @brief Validate frequency range
     * @param frequency Frequency to validate
     * @return true if valid
     */
    bool validateFrequency(uint32_t frequency);

    /**
     * @brief Validate brightness range
     * @param brightness Brightness to validate
     * @return true if valid
     */
    bool validateBrightness(float brightness);
};

#endif // LED_PWM_CONTROL_H
