#include "GPIOControl.h"

GPIOControl::GPIOControl() {
}

bool GPIOControl::begin(bool initialState) {
    if (initialized) {
        return true;
    }

    // Configure GPIO as output
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_GPIO_OUTPUT);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Set initial state
    currentState = initialState;
    gpio_set_level((gpio_num_t)PIN_GPIO_OUTPUT, currentState ? 1 : 0);

    initialized = true;

    Serial.printf("[GPIO] Initialized: Initial state = %s\n", currentState ? "HIGH" : "LOW");

    return true;
}

void GPIOControl::setState(bool state) {
    if (!initialized) {
        return;
    }

    currentState = state;
    gpio_set_level((gpio_num_t)PIN_GPIO_OUTPUT, state ? 1 : 0);
}

void GPIOControl::toggle() {
    setState(!currentState);
}

void GPIOControl::pulse(uint32_t durationMs) {
    if (!initialized) {
        return;
    }

    // Save current state
    bool savedState = currentState;

    // Set HIGH
    setState(true);
    delay(durationMs);

    // Restore previous state
    setState(savedState);
}
