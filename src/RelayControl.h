#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

#include <Arduino.h>
#include "PeripheralPins.h"
#include "driver/gpio.h"

/**
 * @brief Relay Control using GPIO
 *
 * Controls a relay module with:
 * - HIGH active output (HIGH = ON, LOW = OFF)
 * - ON/OFF control
 * - Configurable via web interface
 *
 * Hardware:
 * - GPIO 21: Relay control output
 * - Relay module (voltage-based, with optocoupler recommended)
 *
 * Note: Most relay modules can sink/source sufficient current directly from GPIO.
 * If driving a bare relay coil, use a transistor driver (2N2222, BC547, or MOSFET).
 *
 * Usage:
 *   RelayControl relay;
 *   relay.begin();
 *   relay.setState(true);   // Turn ON
 *   relay.setState(false);  // Turn OFF
 *   relay.toggle();         // Toggle state
 */
class RelayControl {
public:
    /**
     * @brief Constructor
     */
    RelayControl();

    /**
     * @brief Initialize relay control
     * @param initialState Initial relay state (default: false = OFF)
     * @return true if initialization successful
     */
    bool begin(bool initialState = false);

    /**
     * @brief Set relay state
     * @param state true = ON (HIGH), false = OFF (LOW)
     */
    void setState(bool state);

    /**
     * @brief Get current relay state
     * @return true if ON, false if OFF
     */
    bool getState() const { return currentState; }

    /**
     * @brief Toggle relay state
     */
    void toggle();

    /**
     * @brief Turn relay ON
     */
    void turnOn() { setState(true); }

    /**
     * @brief Turn relay OFF
     */
    void turnOff() { setState(false); }

    /**
     * @brief Pulse relay (turn ON for duration, then OFF)
     * @param durationMs Pulse duration in milliseconds
     *
     * Note: This is a blocking function.
     */
    void pulse(uint32_t durationMs);

    /**
     * @brief Check if relay is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return initialized; }

private:
    bool initialized = false;
    bool currentState = false;  // Current relay state (false = OFF, true = ON)
};

#endif // RELAY_CONTROL_H
