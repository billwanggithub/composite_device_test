#include "PeripheralManager.h"
#include "USBCDC.h"

// External reference to USBSerial (defined in main.cpp)
extern USBCDC USBSerial;


PeripheralManager::PeripheralManager() {
}

bool PeripheralManager::begin() {
    // Motor control is now integrated into UART1Mux
    USBSerial.println("\n=== Initializing Peripherals ===");

    // Initialize UART1 (start in disabled mode)
    USBSerial.print("[PeripheralManager] UART1... ");
    uart1.disable();
    USBSerial.println("OK (disabled)");

    // Initialize UART2
    USBSerial.print("[PeripheralManager] UART2... ");
    if (!uart2.begin(115200)) {
        USBSerial.println("FAILED");
        return false;
    }
    USBSerial.println("OK");

    // Initialize User Keys
    USBSerial.print("[PeripheralManager] User Keys... ");
    if (!keys.begin(50, 500, 100)) {  // 50ms debounce, 500ms long press, 100ms repeat
        USBSerial.println("FAILED");
        return false;
    }
    USBSerial.println("OK");

    // Initialize Buzzer
    USBSerial.print("[PeripheralManager] Buzzer... ");
    if (!buzzer.begin(2000, 50.0)) {  // 2kHz, 50% duty
        USBSerial.println("FAILED");
        return false;
    }
    USBSerial.println("OK");

    // Initialize LED PWM
    USBSerial.print("[PeripheralManager] LED PWM... ");
    if (!ledPWM.begin(1000, 50.0)) {  // 1kHz, 50% brightness
        USBSerial.println("FAILED");
        return false;
    }
    USBSerial.println("OK");

    // Initialize Relay
    USBSerial.print("[PeripheralManager] Relay... ");
    if (!relay.begin(false)) {  // Start OFF
        USBSerial.println("FAILED");
        return false;
    }
    USBSerial.println("OK");

    // Initialize General GPIO
    USBSerial.print("[PeripheralManager] GPIO Output... ");
    if (!gpioOut.begin(false)) {  // Start LOW
        USBSerial.println("FAILED");
        return false;
    }
    USBSerial.println("OK");

    allInitialized = true;

    USBSerial.println("=================================");
    USBSerial.println("✅ All peripherals initialized successfully");
    USBSerial.println();
    USBSerial.println("Peripheral Summary:");
    USBSerial.println("  • UART1: GPIO 17 (TX), GPIO 18 (RX) - Multiplexable");
    USBSerial.println("  • UART2: GPIO 43 (TX), GPIO 44 (RX) - Standard");
    USBSerial.println("  • Buzzer: GPIO 13 - PWM (10Hz-20kHz)");
    USBSerial.println("  • LED PWM: GPIO 14 - Brightness control");
    USBSerial.println("  • Relay: GPIO 21 - HIGH active");
    USBSerial.println("  • GPIO Out: GPIO 41 - General purpose");
    USBSerial.println("  • Key 1: GPIO 1 - Duty/Freq increase");
    USBSerial.println("  • Key 2: GPIO 2 - Duty/Freq decrease");
    USBSerial.println("  • Key 3: GPIO 42 - Enter/Start (future)");
    USBSerial.println("=================================\n");

    return true;
}

void PeripheralManager::update() {
    if (!allInitialized) {
        return;
    }

    // Update user keys (debouncing and event detection)
    keys.update();

    // Update UART1 RPM measurement if in PWM/RPM mode
    if (uart1.getMode() == UART1Mux::MODE_PWM_RPM) {
        uart1.updateRPMFrequency();
    }

    // Handle key events (motor control)
    if (keyControlEnabled) {
        handleKeyEvents();
    }
}

void PeripheralManager::setStepSizes(float dutyStep, uint32_t frequencyStep) {
    if (dutyStep > 0.0 && dutyStep <= 100.0) {
        dutyStepSize = dutyStep;
    }

    if (frequencyStep > 0 && frequencyStep <= 100000) {
        frequencyStepSize = frequencyStep;
    }

    USBSerial.printf("[PeripheralManager] Step sizes: Duty=%.2f%%, Freq=%u Hz\n",
                  dutyStepSize, frequencyStepSize);
}

String PeripheralManager::getStatistics() {
    String stats = "Peripheral Statistics:\n";

    // UART1
    stats += "UART1:\n";
    stats += "  Mode: " + String(uart1.getModeName()) + "\n";
    if (uart1.getMode() == UART1Mux::MODE_UART) {
        stats += "  Baud: " + String(uart1.getUARTBaudRate()) + "\n";
        uint32_t tx, rx, err;
        uart1.getUARTStatistics(&tx, &rx, &err);
        stats += "  TX: " + String(tx) + " bytes\n";
        stats += "  RX: " + String(rx) + " bytes\n";
        stats += "  Errors: " + String(err) + "\n";
    } else if (uart1.getMode() == UART1Mux::MODE_PWM_RPM) {
        stats += "  PWM Freq: " + String(uart1.getPWMFrequency()) + " Hz\n";
        stats += "  PWM Duty: " + String(uart1.getPWMDuty(), 1) + "%\n";
        stats += "  RPM Freq: " + String(uart1.getRPMFrequency(), 1) + " Hz\n";
    }

    // UART2
    stats += "UART2:\n";
    stats += "  Baud: " + String(uart2.getBaudRate()) + "\n";
    uint32_t tx, rx, err;
    uart2.getStatistics(&tx, &rx, &err);
    stats += "  TX: " + String(tx) + " bytes\n";
    stats += "  RX: " + String(rx) + " bytes\n";
    stats += "  Errors: " + String(err) + "\n";

    // User Keys
    stats += "User Keys:\n";
    stats += "  Key 1 (Duty+): " + String(keys.getKeyStateName(UserKeys::KEY1)) + "\n";
    stats += "  Key 2 (Duty-): " + String(keys.getKeyStateName(UserKeys::KEY2)) + "\n";
    stats += "  Key 3 (Enter): " + String(keys.getKeyStateName(UserKeys::KEY3)) + "\n";

    // Buzzer
    stats += "Buzzer:\n";
    stats += "  Enabled: " + String(buzzer.isEnabled() ? "Yes" : "No") + "\n";
    stats += "  Frequency: " + String(buzzer.getFrequency()) + " Hz\n";
    stats += "  Duty: " + String(buzzer.getDuty(), 1) + "%\n";

    // LED PWM
    stats += "LED PWM:\n";
    stats += "  Enabled: " + String(ledPWM.isEnabled() ? "Yes" : "No") + "\n";
    stats += "  Frequency: " + String(ledPWM.getFrequency()) + " Hz\n";
    stats += "  Brightness: " + String(ledPWM.getBrightness(), 1) + "%\n";

    // Relay
    stats += "Relay:\n";
    stats += "  State: " + String(relay.getState() ? "ON" : "OFF") + "\n";

    // GPIO
    stats += "GPIO Output:\n";
    stats += "  State: " + String(gpioOut.getState() ? "HIGH" : "LOW") + "\n";

    return stats;
}

// ============================================================================
// Private Helper Functions
// ============================================================================

void PeripheralManager::handleKeyEvents() {
    // Check Key 1 (Increase)
    UserKeys::KeyEvent event1 = keys.getEvent(UserKeys::KEY1);
    if (event1 == UserKeys::EVENT_SHORT_PRESS || event1 == UserKeys::EVENT_REPEAT) {
        if (keyControlAdjustsDuty) {
            adjustMotorDuty(true);
        } else {
            adjustMotorFrequency(true);
        }
    } else if (event1 == UserKeys::EVENT_LONG_PRESS) {
        // Long press: Switch between duty and frequency adjustment
        keyControlAdjustsDuty = !keyControlAdjustsDuty;
        USBSerial.printf("[Keys] Switched to %s adjustment\n",
                     keyControlAdjustsDuty ? "DUTY" : "FREQUENCY");
        // Optional: Beep to confirm mode change
        buzzer.beep(1000, 100);
    }

    // Check Key 2 (Decrease)
    UserKeys::KeyEvent event2 = keys.getEvent(UserKeys::KEY2);
    if (event2 == UserKeys::EVENT_SHORT_PRESS || event2 == UserKeys::EVENT_REPEAT) {
        if (keyControlAdjustsDuty) {
            adjustMotorDuty(false);
        } else {
            adjustMotorFrequency(false);
        }
    } else if (event2 == UserKeys::EVENT_LONG_PRESS) {
        // Long press: Emergency stop
        uart1.setPWMEnabled(false);
        uart1.setPWMDuty(0.0);
        USBSerial.println("[Keys] EMERGENCY STOP triggered by Key 2");
        // Triple beep to indicate emergency stop
        buzzer.beep(2000, 100);
        delay(50);
        buzzer.beep(2000, 100);
        delay(50);
        buzzer.beep(2000, 100);
    }

    // Check Key 3 (Enter/Start) - Future use
    UserKeys::KeyEvent event3 = keys.getEvent(UserKeys::KEY3);
    if (event3 == UserKeys::EVENT_SHORT_PRESS) {
        USBSerial.println("[Keys] Key 3 pressed (reserved for future use)");
        // Optional: Beep acknowledgment
        buzzer.beep(1500, 50);
    } else if (event3 == UserKeys::EVENT_LONG_PRESS) {
        // Long press Key 3: Clear emergency stop
        if (!uart1.isPWMEnabled()) {
            uart1.setPWMEnabled(true);
            USBSerial.println("[Keys] Emergency stop CLEARED by Key 3");
            // Confirmation beep
            buzzer.beep(1000, 200);
        }
    }
}

void PeripheralManager::adjustMotorDuty(bool increase) {
    // Motor control now handled by UART1
    float currentDuty = uart1.getPWMDuty();
    float newDuty = currentDuty + (increase ? dutyStepSize : -dutyStepSize);

    // Clamp to valid range
    if (newDuty < 0.0) newDuty = 0.0;
    if (newDuty > 100.0) newDuty = 100.0;

    if (newDuty != currentDuty) {
        uart1.setPWMDuty(newDuty);
        USBSerial.printf("[Keys] Duty adjusted: %.1f%% → %.1f%%\n", currentDuty, newDuty);
    }
}

void PeripheralManager::adjustMotorFrequency(bool increase) {
    // Motor control now handled by UART1
    uint32_t currentFreq = uart1.getPWMFrequency();
    int32_t newFreq = currentFreq + (increase ? (int32_t)frequencyStepSize : -(int32_t)frequencyStepSize);

    // Clamp to valid range (10 Hz - 500 kHz, based on motor control limits)
    if (newFreq < 10) newFreq = 10;
    if (newFreq > 500000) newFreq = 500000;

    if ((uint32_t)newFreq != currentFreq) {
        uart1.setPWMFrequency((uint32_t)newFreq);
        USBSerial.printf("[Keys] Frequency adjusted: %u Hz → %u Hz\n", currentFreq, (uint32_t)newFreq);
    }
}

// ============================================================================
// Settings Management Implementation
// ============================================================================

bool PeripheralManager::beginSettings() {
    if (!settingsManager.begin()) {
        USBSerial.println("[PeripheralManager] Failed to initialize settings manager");
        return false;
    }
    USBSerial.println("[PeripheralManager] Settings manager initialized");
    return true;
}

bool PeripheralManager::loadSettings() {
    if (!settingsManager.load()) {
        USBSerial.println("[PeripheralManager] Failed to load settings");
        return false;
    }

    USBSerial.println("[PeripheralManager] Settings loaded successfully");
    return true;
}

bool PeripheralManager::saveSettings() {
    // Capture current peripheral states into settings
    PeripheralSettings& settings = settingsManager.get();

    // UART1 state
    switch (uart1.getMode()) {
        case UART1Mux::MODE_DISABLED:
            settings.uart1Mode = 0;
            break;
        case UART1Mux::MODE_UART:
            settings.uart1Mode = 1;
            settings.uart1Baud = uart1.getUARTBaudRate();
            break;
        case UART1Mux::MODE_PWM_RPM:
            settings.uart1Mode = 2;
            settings.uart1PwmFreq = uart1.getPWMFrequency();
            settings.uart1PwmDuty = uart1.getPWMDuty();
            settings.uart1PwmEnabled = uart1.isPWMEnabled();
            break;
    }

    // UART2 state
    settings.uart2Baud = uart2.getBaudRate();

    // Buzzer state
    settings.buzzerFreq = buzzer.getFrequency();
    settings.buzzerDuty = buzzer.getDuty();
    settings.buzzerEnabled = buzzer.isEnabled();

    // LED PWM state
    settings.ledPwmFreq = ledPWM.getFrequency();
    settings.ledBrightness = ledPWM.getBrightness();
    settings.ledEnabled = ledPWM.isEnabled();

    // Relay state
    settings.relayState = relay.getState();

    // GPIO state
    settings.gpioState = gpioOut.getState();

    // Key control settings
    settings.keyControlAdjustDuty = keyControlAdjustsDuty;
    settings.keyDutyStep = dutyStepSize;
    settings.keyFreqStep = frequencyStepSize;
    settings.keyControlEnabled = keyControlEnabled;

    // Save to NVS
    if (!settingsManager.save()) {
        USBSerial.println("[PeripheralManager] Failed to save settings");
        return false;
    }

    USBSerial.println("[PeripheralManager] Settings saved successfully");
    return true;
}

bool PeripheralManager::applySettings() {
    const PeripheralSettings& settings = settingsManager.get();

    USBSerial.println("[PeripheralManager] Applying settings to peripherals...");

    // UART1 mode is NOT applied from NVS settings
    // It always defaults to PWM/RPM mode at startup (set in main.cpp)
    // and can only be changed via commands (UART1 MODE <UART|PWM|OFF>)
    // This ensures UART1 always starts in PWM/RPM mode regardless of saved settings
    USBSerial.println("[PeripheralManager] UART1: Mode not applied (uses startup default PWM/RPM)");

    // Apply Buzzer settings
    buzzer.setFrequency(settings.buzzerFreq);
    buzzer.setDuty(settings.buzzerDuty);
    buzzer.enable(settings.buzzerEnabled);
    USBSerial.printf("[PeripheralManager] Buzzer: %u Hz, %.1f%%, %s\n",
        settings.buzzerFreq, settings.buzzerDuty,
        settings.buzzerEnabled ? "enabled" : "disabled");

    // Apply LED PWM settings
    ledPWM.setFrequency(settings.ledPwmFreq);
    ledPWM.setBrightness(settings.ledBrightness);
    ledPWM.enable(settings.ledEnabled);
    USBSerial.printf("[PeripheralManager] LED PWM: %u Hz, %.1f%%, %s\n",
        settings.ledPwmFreq, settings.ledBrightness,
        settings.ledEnabled ? "enabled" : "disabled");

    // Apply Relay settings
    relay.setState(settings.relayState);
    USBSerial.printf("[PeripheralManager] Relay: %s\n", settings.relayState ? "ON" : "OFF");

    // Apply GPIO settings
    if (settings.gpioState) {
        gpioOut.setHigh();
    } else {
        gpioOut.setLow();
    }
    USBSerial.printf("[PeripheralManager] GPIO: %s\n", settings.gpioState ? "HIGH" : "LOW");

    // Apply key control settings
    keyControlAdjustsDuty = settings.keyControlAdjustDuty;
    dutyStepSize = settings.keyDutyStep;
    frequencyStepSize = settings.keyFreqStep;
    keyControlEnabled = settings.keyControlEnabled;
    USBSerial.printf("[PeripheralManager] Key Control: %s mode, duty step=%.1f%%, freq step=%u Hz, %s\n",
        settings.keyControlAdjustDuty ? "duty" : "frequency",
        settings.keyDutyStep, settings.keyFreqStep,
        settings.keyControlEnabled ? "enabled" : "disabled");

    USBSerial.println("[PeripheralManager] All settings applied successfully");
    return true;
}

void PeripheralManager::resetSettings() {
    settingsManager.reset();
    USBSerial.println("[PeripheralManager] Settings reset to defaults");
}
