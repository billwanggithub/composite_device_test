#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#include <Arduino.h>
#include "PeripheralPins.h"
#include "driver/gpio.h"

/**
 * @brief General Purpose GPIO Output Control
 *
 * Controls a general-purpose GPIO output with:
 * - HIGH/LOW output control
 * - Configurable via web interface
 * - Future expansion for custom logic
 *
 * Hardware:
 * - GPIO 41: General purpose output
 *
 * Usage:
 *   GPIOControl gpio;
 *   gpio.begin();
 *   gpio.setHigh();     // Set HIGH
 *   gpio.setLow();      // Set LOW
 *   gpio.toggle();      // Toggle state
 */
class GPIOControl {
public:
    /**
     * @brief Constructor
     */
    GPIOControl();

    /**
     * @brief Initialize GPIO control
     * @param initialState Initial GPIO state (default: false = LOW)
     * @return true if initialization successful
     */
    bool begin(bool initialState = false);

    /**
     * @brief Set GPIO state
     * @param state true = HIGH, false = LOW
     */
    void setState(bool state);

    /**
     * @brief Get current GPIO state
     * @return true if HIGH, false if LOW
     */
    bool getState() const { return currentState; }

    /**
     * @brief Toggle GPIO state
     */
    void toggle();

    /**
     * @brief Set GPIO HIGH
     */
    void setHigh() { setState(true); }

    /**
     * @brief Set GPIO LOW
     */
    void setLow() { setState(false); }

    /**
     * @brief Pulse GPIO (set HIGH for duration, then LOW)
     * @param durationMs Pulse duration in milliseconds
     *
     * Note: This is a blocking function.
     */
    void pulse(uint32_t durationMs);

    /**
     * @brief Check if GPIO is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return initialized; }

private:
    bool initialized = false;
    bool currentState = false;  // Current GPIO state (false = LOW, true = HIGH)
};

#endif // GPIO_CONTROL_H
