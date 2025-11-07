#include "UserKeys.h"
#include "driver/gpio.h"

UserKeys::UserKeys() {
}

bool UserKeys::begin(uint32_t debounceMs, uint32_t longPressMs, uint32_t repeatIntervalMs) {
    if (initialized) {
        return true;
    }

    // Save timing configuration
    debounceTime = debounceMs;
    longPressTime = longPressMs;
    repeatInterval = repeatIntervalMs;

    // Configure GPIO pins
    for (int i = 0; i < KEY_COUNT; i++) {
        // Configure as input with internal pull-up (Active LOW)
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << keyPins[i]);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);

        // Initialize key state
        keyStates[i].currentState = false;
        keyStates[i].previousState = false;
        keyStates[i].rawState = readRawKey((Key)i);
        keyStates[i].lastChangeTime = millis();
        keyStates[i].pressStartTime = 0;
        keyStates[i].longPressTriggered = false;
        keyStates[i].lastRepeatTime = 0;
        keyStates[i].pendingEvent = EVENT_NONE;
    }

    initialized = true;
    Serial.println("[UserKeys] Initialized: 3 keys with debouncing and long-press detection");

    return true;
}

void UserKeys::update() {
    if (!initialized) {
        return;
    }

    for (int i = 0; i < KEY_COUNT; i++) {
        updateKey((Key)i);
    }
}

bool UserKeys::isPressed(Key key) {
    if (!isValidKey(key)) {
        return false;
    }
    return keyStates[key].currentState;
}

bool UserKeys::wasPressed(Key key) {
    if (!isValidKey(key)) {
        return false;
    }

    KeyState& state = keyStates[key];

    // Edge detection: current=pressed, previous=released
    if (state.currentState && !state.previousState) {
        return true;
    }

    return false;
}

bool UserKeys::wasReleased(Key key) {
    if (!isValidKey(key)) {
        return false;
    }

    KeyState& state = keyStates[key];

    // Edge detection: current=released, previous=pressed
    if (!state.currentState && state.previousState) {
        return true;
    }

    return false;
}

UserKeys::KeyEvent UserKeys::getEvent(Key key) {
    if (!isValidKey(key)) {
        return EVENT_NONE;
    }

    KeyEvent event = keyStates[key].pendingEvent;
    keyStates[key].pendingEvent = EVENT_NONE;  // Clear after reading
    return event;
}

unsigned long UserKeys::getPressDuration(Key key) {
    if (!isValidKey(key)) {
        return 0;
    }

    const KeyState& state = keyStates[key];

    if (!state.currentState) {
        return 0;  // Not pressed
    }

    return millis() - state.pressStartTime;
}

void UserKeys::clearEvents() {
    for (int i = 0; i < KEY_COUNT; i++) {
        keyStates[i].pendingEvent = EVENT_NONE;
    }
}

void UserKeys::configureTiming(uint32_t debounceMs, uint32_t longPressMs, uint32_t repeatIntervalMs) {
    debounceTime = debounceMs;
    longPressTime = longPressMs;
    repeatInterval = repeatIntervalMs;
}

const char* UserKeys::getKeyStateName(Key key) {
    if (!isValidKey(key)) {
        return "UNKNOWN";
    }
    return keyStates[key].currentState ? "PRESSED" : "RELEASED";
}

const char* UserKeys::getEventName(KeyEvent event) {
    switch (event) {
        case EVENT_NONE: return "NONE";
        case EVENT_SHORT_PRESS: return "SHORT_PRESS";
        case EVENT_LONG_PRESS: return "LONG_PRESS";
        case EVENT_REPEAT: return "REPEAT";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Private Helper Functions
// ============================================================================

bool UserKeys::readRawKey(Key key) {
    if (!isValidKey(key)) {
        return false;
    }

    // Read GPIO (Active LOW: LOW = pressed, HIGH = released)
    int level = gpio_get_level((gpio_num_t)keyPins[key]);
    return (level == 0);  // true if pressed
}

void UserKeys::updateKey(Key key) {
    if (!isValidKey(key)) {
        return;
    }

    KeyState& state = keyStates[key];
    unsigned long now = millis();

    // Read raw GPIO state
    bool rawPressed = readRawKey(key);
    state.rawState = rawPressed;

    // Debouncing logic
    if (rawPressed != state.currentState) {
        // State is different from debounced state
        if (now - state.lastChangeTime >= debounceTime) {
            // Debounce time elapsed, accept new state
            state.previousState = state.currentState;
            state.currentState = rawPressed;
            state.lastChangeTime = now;

            // Detect press/release events
            if (rawPressed && !state.previousState) {
                // Key just pressed
                state.pressStartTime = now;
                state.longPressTriggered = false;
                state.lastRepeatTime = now;
                // Don't set event yet (wait for release or long press)
            }
            else if (!rawPressed && state.previousState) {
                // Key just released
                unsigned long pressDuration = now - state.pressStartTime;

                if (!state.longPressTriggered) {
                    // Short press (released before long press threshold)
                    if (pressDuration < longPressTime) {
                        state.pendingEvent = EVENT_SHORT_PRESS;
                    }
                }
                // else: Long press was already triggered, no event on release
            }
        }
        // else: Still bouncing, ignore
    } else {
        // State is stable (matches debounced state)
        state.lastChangeTime = now;

        // Check for long press event
        if (state.currentState && !state.longPressTriggered) {
            unsigned long pressDuration = now - state.pressStartTime;

            if (pressDuration >= longPressTime) {
                // Long press threshold reached
                state.longPressTriggered = true;
                state.pendingEvent = EVENT_LONG_PRESS;
                state.lastRepeatTime = now;
            }
        }

        // Check for repeat event (during long press)
        if (state.currentState && state.longPressTriggered) {
            if (now - state.lastRepeatTime >= repeatInterval) {
                state.pendingEvent = EVENT_REPEAT;
                state.lastRepeatTime = now;
            }
        }
    }
}
