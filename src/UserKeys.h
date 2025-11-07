#ifndef USER_KEYS_H
#define USER_KEYS_H

#include <Arduino.h>
#include "PeripheralPins.h"

/**
 * @brief User Keys Manager with debouncing and long-press detection
 *
 * Manages three user keys with:
 * - Active LOW detection (internal pull-up enabled)
 * - Software debouncing (50ms default)
 * - Short press detection (< 500ms)
 * - Long press detection (≥ 500ms)
 * - Continuous repeat during long press (every 100ms)
 *
 * Keys:
 * - Key 1 (GPIO 1): Duty/Frequency increase
 * - Key 2 (GPIO 2): Duty/Frequency decrease
 * - Key 3 (GPIO 42): Enter/Start (future use)
 *
 * Usage:
 *   UserKeys keys;
 *   keys.begin();
 *
 *   // In main loop:
 *   keys.update();
 *
 *   if (keys.isPressed(UserKeys::KEY1)) {
 *       // Handle Key 1 press
 *   }
 */
class UserKeys {
public:
    /**
     * @brief Key enumeration
     */
    enum Key {
        KEY1 = 0,  ///< Key 1 - Duty/Frequency increase
        KEY2 = 1,  ///< Key 2 - Duty/Frequency decrease
        KEY3 = 2,  ///< Key 3 - Enter/Start
        KEY_COUNT = 3
    };

    /**
     * @brief Key event enumeration
     */
    enum KeyEvent {
        EVENT_NONE,         ///< No event
        EVENT_SHORT_PRESS,  ///< Short press (< 500ms)
        EVENT_LONG_PRESS,   ///< Long press (≥ 500ms, triggered once)
        EVENT_REPEAT        ///< Repeat event during long press (every 100ms)
    };

    /**
     * @brief Constructor
     */
    UserKeys();

    /**
     * @brief Initialize user keys
     * @param debounceMs Debounce time in milliseconds (default: 50ms)
     * @param longPressMs Long press threshold in milliseconds (default: 500ms)
     * @param repeatIntervalMs Repeat interval during long press in milliseconds (default: 100ms)
     * @return true if initialization successful
     */
    bool begin(uint32_t debounceMs = 50,
               uint32_t longPressMs = 500,
               uint32_t repeatIntervalMs = 100);

    /**
     * @brief Update key states (call regularly in main loop or task)
     *
     * This function should be called frequently (e.g., every 10-20ms)
     * to ensure proper debouncing and event detection.
     */
    void update();

    /**
     * @brief Check if key is currently pressed
     * @param key Key to check
     * @return true if key is pressed
     */
    bool isPressed(Key key);

    /**
     * @brief Check if key was just pressed (edge detection)
     * @param key Key to check
     * @return true if key was just pressed
     *
     * This function returns true only once per press event.
     * Call update() before calling this function.
     */
    bool wasPressed(Key key);

    /**
     * @brief Check if key was just released (edge detection)
     * @param key Key to check
     * @return true if key was just released
     *
     * This function returns true only once per release event.
     */
    bool wasReleased(Key key);

    /**
     * @brief Get key event (short press, long press, or repeat)
     * @param key Key to check
     * @return Key event type
     *
     * This function returns the latest event and clears it.
     * Call update() before calling this function.
     */
    KeyEvent getEvent(Key key);

    /**
     * @brief Get key press duration
     * @param key Key to check
     * @return Press duration in milliseconds (0 if not pressed)
     */
    unsigned long getPressDuration(Key key);

    /**
     * @brief Clear all pending events
     */
    void clearEvents();

    /**
     * @brief Configure timing parameters
     * @param debounceMs Debounce time in milliseconds
     * @param longPressMs Long press threshold in milliseconds
     * @param repeatIntervalMs Repeat interval during long press
     */
    void configureTiming(uint32_t debounceMs, uint32_t longPressMs, uint32_t repeatIntervalMs);

    /**
     * @brief Get debounce time
     * @return Debounce time in milliseconds
     */
    uint32_t getDebounceTime() const { return debounceTime; }

    /**
     * @brief Get long press threshold
     * @return Long press threshold in milliseconds
     */
    uint32_t getLongPressTime() const { return longPressTime; }

    /**
     * @brief Get repeat interval
     * @return Repeat interval in milliseconds
     */
    uint32_t getRepeatInterval() const { return repeatInterval; }

    /**
     * @brief Get key state as string
     * @param key Key to check
     * @return State string ("PRESSED", "RELEASED", or "UNKNOWN")
     */
    const char* getKeyStateName(Key key);

    /**
     * @brief Get event name as string
     * @param event Event type
     * @return Event name string
     */
    const char* getEventName(KeyEvent event);

    /**
     * @brief Check if keys are initialized
     * @return true if initialized
     */
    bool isInitialized() const { return initialized; }

private:
    struct KeyState {
        bool currentState = false;        // Current debounced state (true = pressed)
        bool previousState = false;       // Previous state for edge detection
        bool rawState = false;            // Current raw GPIO reading
        unsigned long lastChangeTime = 0; // Last state change time
        unsigned long pressStartTime = 0; // Press start time
        bool longPressTriggered = false;  // Long press event already triggered
        unsigned long lastRepeatTime = 0; // Last repeat event time
        KeyEvent pendingEvent = EVENT_NONE; // Pending event
    };

    bool initialized = false;

    // Timing configuration
    uint32_t debounceTime = 50;       // Debounce time in ms
    uint32_t longPressTime = 500;     // Long press threshold in ms
    uint32_t repeatInterval = 100;    // Repeat interval in ms

    // Key states
    KeyState keyStates[KEY_COUNT];

    // GPIO pin mapping
    const int keyPins[KEY_COUNT] = {
        PIN_USER_KEY1,  // Key 1
        PIN_USER_KEY2,  // Key 2
        PIN_USER_KEY3   // Key 3
    };

    /**
     * @brief Read raw key state from GPIO
     * @param key Key to read
     * @return true if pressed (LOW), false if released (HIGH)
     */
    bool readRawKey(Key key);

    /**
     * @brief Update single key state
     * @param key Key to update
     */
    void updateKey(Key key);

    /**
     * @brief Validate key index
     * @param key Key to validate
     * @return true if valid
     */
    bool isValidKey(Key key) {
        return (key >= 0 && key < KEY_COUNT);
    }
};

#endif // USER_KEYS_H
