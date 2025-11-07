#include "RelayControl.h"

RelayControl::RelayControl() {
}

bool RelayControl::begin(bool initialState) {
    if (initialized) {
        return true;
    }

    // Configure GPIO as output
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_RELAY_CONTROL);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Set initial state
    currentState = initialState;
    gpio_set_level((gpio_num_t)PIN_RELAY_CONTROL, currentState ? 1 : 0);

    initialized = true;

    Serial.printf("[Relay] Initialized: Initial state = %s\n", currentState ? "ON" : "OFF");

    return true;
}

void RelayControl::setState(bool state) {
    if (!initialized) {
        return;
    }

    currentState = state;
    gpio_set_level((gpio_num_t)PIN_RELAY_CONTROL, state ? 1 : 0);

    Serial.printf("[Relay] State changed: %s\n", state ? "ON" : "OFF");
}

void RelayControl::toggle() {
    setState(!currentState);
}

void RelayControl::pulse(uint32_t durationMs) {
    if (!initialized) {
        return;
    }

    // Save current state
    bool savedState = currentState;

    // Turn ON
    setState(true);
    delay(durationMs);

    // Restore previous state
    setState(savedState);
}
