#include "CommandParser.h"
#include "PeripheralManager.h"

// External reference to peripheral manager (defined in main.cpp)
extern PeripheralManager peripheralManager;

// ============================================================================
// UART1 Commands
// ============================================================================

void CommandParser::handleUART1Mode(const String& cmd, ICommandResponse* response) {
    // Extract mode parameter
    int spaceIndex = cmd.lastIndexOf(' ');  // Find last space to get the mode parameter
    if (spaceIndex == -1) {
        response->println("Usage: UART1 MODE <UART|PWM|OFF>");
        return;
    }

    String mode = cmd.substring(spaceIndex + 1);
    mode.trim();
    mode.toUpperCase();

    if (mode == "UART") {
        if (peripheralManager.getUART1().setModeUART(115200)) {
            response->println("UART1 switched to UART mode (115200 baud)");
        } else {
            response->println("ERROR: Failed to switch UART1 to UART mode");
        }
    } else if (mode == "PWM") {
        if (peripheralManager.getUART1().setModePWM_RPM()) {
            response->println("UART1 switched to PWM/RPM mode");
        } else {
            response->println("ERROR: Failed to switch UART1 to PWM/RPM mode");
        }
    } else if (mode == "OFF") {
        peripheralManager.getUART1().disable();
        response->println("UART1 disabled");
    } else {
        response->println("ERROR: Invalid mode. Use UART, PWM, or OFF");
    }
}

void CommandParser::handleUART1Config(const String& cmd, ICommandResponse* response) {
    // UART1 CONFIG <baud> [stop_bits] [parity]
    // Parse parameters
    int idx = cmd.indexOf(' ', 13);  // After "UART1 CONFIG "
    if (idx == -1) {
        response->println("Usage: UART1 CONFIG <baud> [1|2] [N|E|O]");
        return;
    }

    String params = cmd.substring(idx + 1);
    int baud = params.toInt();

    if (baud < 2400 || baud > 1500000) {
        response->println("ERROR: Baud rate must be 2400-1500000");
        return;
    }

    // Optional: Parse stop bits and parity (default: 1 stop bit, no parity)
    uart_stop_bits_t stopBits = UART_STOP_BITS_1;
    uart_parity_t parity = UART_PARITY_DISABLE;

    if (peripheralManager.getUART1().reconfigureUART(baud, stopBits, parity)) {
        response->printf("UART1 configured: %d baud\n", baud);
    } else {
        response->println("ERROR: Failed to configure UART1");
    }
}

void CommandParser::handleUART1PWM(const String& cmd, ICommandResponse* response) {
    // UART1 PWM <freq> <duty>
    int idx1 = cmd.indexOf(' ', 10);  // After "UART1 PWM "
    if (idx1 == -1) {
        response->println("Usage: UART1 PWM <freq> <duty>");
        return;
    }

    int idx2 = cmd.indexOf(' ', idx1 + 1);
    if (idx2 == -1) {
        response->println("Usage: UART1 PWM <freq> <duty>");
        return;
    }

    uint32_t freq = cmd.substring(idx1 + 1, idx2).toInt();
    float duty = cmd.substring(idx2 + 1).toFloat();

    if (peripheralManager.getUART1().setPWMFrequency(freq) &&
        peripheralManager.getUART1().setPWMDuty(duty)) {
        response->printf("UART1 PWM: %u Hz, %.1f%% duty\n", freq, duty);
        peripheralManager.getUART1().setPWMEnabled(true);
    } else {
        response->println("ERROR: Failed to set UART1 PWM parameters");
    }
}

void CommandParser::handleUART1Status(ICommandResponse* response) {
    auto& uart1 = peripheralManager.getUART1();

    response->println("UART1 Status:");
    response->printf("  Mode: %s\n", uart1.getModeName());

    if (uart1.getMode() == UART1Mux::MODE_UART) {
        response->printf("  Baud: %u\n", uart1.getUARTBaudRate());
        uint32_t tx, rx, err;
        uart1.getUARTStatistics(&tx, &rx, &err);
        response->printf("  TX: %u bytes, RX: %u bytes, Errors: %u\n", tx, rx, err);
    } else if (uart1.getMode() == UART1Mux::MODE_PWM_RPM) {
        response->printf("  PWM Frequency: %u Hz\n", uart1.getPWMFrequency());
        response->printf("  PWM Duty: %.1f%%\n", uart1.getPWMDuty());
        response->printf("  PWM Enabled: %s\n", uart1.isPWMEnabled() ? "Yes" : "No");
        response->printf("  RPM Frequency: %.1f Hz\n", uart1.getRPMFrequency());
        response->printf("  RPM Signal: %s\n", uart1.hasRPMSignal() ? "Present" : "None");
    }
}

void CommandParser::handleUART1Write(const String& cmd, ICommandResponse* response) {
    // UART1 WRITE <text>
    int idx = cmd.indexOf(' ', 12);  // After "UART1 WRITE "
    if (idx == -1) {
        response->println("Usage: UART1 WRITE <text>");
        return;
    }

    String text = cmd.substring(idx + 1);
    text += "\n";  // Add newline

    int written = peripheralManager.getUART1().write(text.c_str());
    if (written > 0) {
        response->printf("Wrote %d bytes to UART1\n", written);
    } else {
        response->println("ERROR: Failed to write to UART1");
    }
}

// ============================================================================
// UART2 Commands
// ============================================================================

void CommandParser::handleUART2Config(const String& cmd, ICommandResponse* response) {
    // UART2 CONFIG <baud>
    int idx = cmd.indexOf(' ', 13);
    if (idx == -1) {
        response->println("Usage: UART2 CONFIG <baud>");
        return;
    }

    uint32_t baud = cmd.substring(idx + 1).toInt();

    if (baud < 2400 || baud > 1500000) {
        response->println("ERROR: Baud rate must be 2400-1500000");
        return;
    }

    if (peripheralManager.getUART2().reconfigure(baud)) {
        response->printf("UART2 configured: %u baud\n", baud);
    } else {
        response->println("ERROR: Failed to configure UART2");
    }
}

void CommandParser::handleUART2Status(ICommandResponse* response) {
    auto& uart2 = peripheralManager.getUART2();

    response->println("UART2 Status:");
    response->printf("  Baud: %u\n", uart2.getBaudRate());
    uint32_t tx, rx, err;
    uart2.getStatistics(&tx, &rx, &err);
    response->printf("  TX: %u bytes, RX: %u bytes, Errors: %u\n", tx, rx, err);
}

void CommandParser::handleUART2Write(const String& cmd, ICommandResponse* response) {
    // UART2 WRITE <text>
    int idx = cmd.indexOf(' ', 12);
    if (idx == -1) {
        response->println("Usage: UART2 WRITE <text>");
        return;
    }

    String text = cmd.substring(idx + 1);
    text += "\n";

    int written = peripheralManager.getUART2().write(text.c_str());
    if (written > 0) {
        response->printf("Wrote %d bytes to UART2\n", written);
    } else {
        response->println("ERROR: Failed to write to UART2");
    }
}

// ============================================================================
// Buzzer Commands
// ============================================================================

void CommandParser::handleBuzzerControl(const String& cmd, ICommandResponse* response) {
    // BUZZER <freq> <duty> or BUZZER ON/OFF
    int idx = cmd.indexOf(' ', 7);  // After "BUZZER "
    if (idx == -1) {
        response->println("Usage: BUZZER <freq> <duty> | BUZZER ON | BUZZER OFF");
        return;
    }

    String param = cmd.substring(idx + 1);
    param.trim();
    param.toUpperCase();

    if (param == "ON") {
        peripheralManager.getBuzzer().enable(true);
        response->println("Buzzer enabled");
    } else if (param == "OFF") {
        peripheralManager.getBuzzer().enable(false);
        response->println("Buzzer disabled");
    } else {
        // Parse frequency and duty
        int idx2 = param.indexOf(' ');
        if (idx2 == -1) {
            response->println("Usage: BUZZER <freq> <duty>");
            return;
        }

        uint32_t freq = param.substring(0, idx2).toInt();
        float duty = param.substring(idx2 + 1).toFloat();

        if (peripheralManager.getBuzzer().setFrequency(freq) &&
            peripheralManager.getBuzzer().setDuty(duty)) {
            response->printf("Buzzer: %u Hz, %.1f%% duty\n", freq, duty);
        } else {
            response->println("ERROR: Invalid buzzer parameters");
        }
    }
}

void CommandParser::handleBuzzerBeep(const String& cmd, ICommandResponse* response) {
    // BUZZER BEEP <freq> <duration_ms>
    int idx1 = cmd.indexOf(' ', 12);  // After "BUZZER BEEP "
    if (idx1 == -1) {
        response->println("Usage: BUZZER BEEP <freq> <duration_ms>");
        return;
    }

    int idx2 = cmd.indexOf(' ', idx1 + 1);
    if (idx2 == -1) {
        response->println("Usage: BUZZER BEEP <freq> <duration_ms>");
        return;
    }

    uint32_t freq = cmd.substring(idx1 + 1, idx2).toInt();
    uint32_t duration = cmd.substring(idx2 + 1).toInt();

    peripheralManager.getBuzzer().beep(freq, duration);
    response->printf("Beep: %u Hz for %u ms\n", freq, duration);
}

// ============================================================================
// LED PWM Commands
// ============================================================================

void CommandParser::handleLEDPWM(const String& cmd, ICommandResponse* response) {
    // LED_PWM <freq> <brightness> or LED_PWM ON/OFF
    int idx = cmd.indexOf(' ', 8);  // After "LED_PWM "
    if (idx == -1) {
        response->println("Usage: LED_PWM <freq> <brightness> | LED_PWM ON | LED_PWM OFF");
        return;
    }

    String param = cmd.substring(idx + 1);
    param.trim();
    param.toUpperCase();

    if (param == "ON") {
        peripheralManager.getLEDPWM().enable(true);
        response->println("LED PWM enabled");
    } else if (param == "OFF") {
        peripheralManager.getLEDPWM().enable(false);
        response->println("LED PWM disabled");
    } else {
        int idx2 = param.indexOf(' ');
        if (idx2 == -1) {
            response->println("Usage: LED_PWM <freq> <brightness>");
            return;
        }

        uint32_t freq = param.substring(0, idx2).toInt();
        float brightness = param.substring(idx2 + 1).toFloat();

        if (peripheralManager.getLEDPWM().setFrequency(freq) &&
            peripheralManager.getLEDPWM().setBrightness(brightness)) {
            response->printf("LED PWM: %u Hz, %.1f%% brightness\n", freq, brightness);
        } else {
            response->println("ERROR: Invalid LED PWM parameters");
        }
    }
}

void CommandParser::handleLEDFade(const String& cmd, ICommandResponse* response) {
    // LED_PWM FADE <brightness> <time_ms>
    int idx1 = cmd.indexOf(' ', 13);  // After "LED_PWM FADE "
    if (idx1 == -1) {
        response->println("Usage: LED_PWM FADE <brightness> <time_ms>");
        return;
    }

    int idx2 = cmd.indexOf(' ', idx1 + 1);
    if (idx2 == -1) {
        response->println("Usage: LED_PWM FADE <brightness> <time_ms>");
        return;
    }

    float brightness = cmd.substring(idx1 + 1, idx2).toFloat();
    uint32_t time = cmd.substring(idx2 + 1).toInt();

    peripheralManager.getLEDPWM().fadeTo(brightness, time);
    response->printf("Fading LED to %.1f%% over %u ms\n", brightness, time);
}

// ============================================================================
// Relay Commands
// ============================================================================

void CommandParser::handleRelayControl(const String& cmd, ICommandResponse* response) {
    // RELAY ON/OFF/TOGGLE/PULSE <duration_ms>
    int idx = cmd.indexOf(' ', 6);  // After "RELAY "
    if (idx == -1) {
        response->println("Usage: RELAY ON | RELAY OFF | RELAY TOGGLE | RELAY PULSE <ms>");
        return;
    }

    String param = cmd.substring(idx + 1);
    param.trim();
    param.toUpperCase();

    if (param == "ON") {
        peripheralManager.getRelay().turnOn();
        response->println("Relay ON");
    } else if (param == "OFF") {
        peripheralManager.getRelay().turnOff();
        response->println("Relay OFF");
    } else if (param == "TOGGLE") {
        peripheralManager.getRelay().toggle();
        response->printf("Relay toggled: %s\n",
                        peripheralManager.getRelay().getState() ? "ON" : "OFF");
    } else if (param.startsWith("PULSE")) {
        int idx2 = param.indexOf(' ');
        if (idx2 == -1) {
            response->println("Usage: RELAY PULSE <duration_ms>");
            return;
        }
        uint32_t duration = param.substring(idx2 + 1).toInt();
        peripheralManager.getRelay().pulse(duration);
        response->printf("Relay pulsed for %u ms\n", duration);
    } else {
        response->println("ERROR: Invalid parameter. Use ON, OFF, TOGGLE, or PULSE <ms>");
    }
}

// ============================================================================
// GPIO Commands
// ============================================================================

void CommandParser::handleGPIOControl(const String& cmd, ICommandResponse* response) {
    // GPIO HIGH/LOW/TOGGLE/STATUS
    int idx = cmd.indexOf(' ', 5);  // After "GPIO "
    if (idx == -1) {
        response->println("Usage: GPIO HIGH | GPIO LOW | GPIO TOGGLE | GPIO STATUS");
        return;
    }

    String param = cmd.substring(idx + 1);
    param.trim();
    param.toUpperCase();

    if (param == "HIGH") {
        peripheralManager.getGPIO().setHigh();
        response->println("GPIO set HIGH");
    } else if (param == "LOW") {
        peripheralManager.getGPIO().setLow();
        response->println("GPIO set LOW");
    } else if (param == "TOGGLE") {
        peripheralManager.getGPIO().toggle();
        response->printf("GPIO toggled: %s\n",
                        peripheralManager.getGPIO().getState() ? "HIGH" : "LOW");
    } else if (param == "STATUS") {
        response->printf("GPIO: %s\n",
                        peripheralManager.getGPIO().getState() ? "HIGH" : "LOW");
    } else {
        response->println("ERROR: Invalid parameter. Use HIGH, LOW, TOGGLE, or STATUS");
    }
}

// ============================================================================
// Keys Commands
// ============================================================================

void CommandParser::handleKeysStatus(ICommandResponse* response) {
    auto& keys = peripheralManager.getKeys();

    response->println("User Keys Status:");
    response->printf("  Key 1 (Duty+): %s\n", keys.getKeyStateName(UserKeys::KEY1));
    response->printf("  Key 2 (Duty-): %s\n", keys.getKeyStateName(UserKeys::KEY2));
    response->printf("  Key 3 (Enter): %s\n", keys.getKeyStateName(UserKeys::KEY3));
    response->printf("  Control Enabled: %s\n",
                    peripheralManager.isKeyControlEnabled() ? "Yes" : "No");
    response->printf("  Control Mode: %s\n",
                    peripheralManager.isKeyControlAdjustingDuty() ? "Duty" : "Frequency");
    response->printf("  Duty Step: %.2f%%\n", peripheralManager.getDutyStep());
    response->printf("  Frequency Step: %u Hz\n", peripheralManager.getFrequencyStep());
}

void CommandParser::handleKeysConfig(const String& cmd, ICommandResponse* response) {
    // KEYS CONFIG <duty_step> <freq_step>
    int idx1 = cmd.indexOf(' ', 12);  // After "KEYS CONFIG "
    if (idx1 == -1) {
        response->println("Usage: KEYS CONFIG <duty_step> <freq_step>");
        return;
    }

    int idx2 = cmd.indexOf(' ', idx1 + 1);
    if (idx2 == -1) {
        response->println("Usage: KEYS CONFIG <duty_step> <freq_step>");
        return;
    }

    float dutyStep = cmd.substring(idx1 + 1, idx2).toFloat();
    uint32_t freqStep = cmd.substring(idx2 + 1).toInt();

    peripheralManager.setStepSizes(dutyStep, freqStep);
    response->printf("Key step sizes: Duty=%.2f%%, Freq=%u Hz\n", dutyStep, freqStep);
}

void CommandParser::handleKeysMode(const String& cmd, ICommandResponse* response) {
    // KEYS MODE <DUTY|FREQ>
    int idx = cmd.lastIndexOf(' ');  // Find last space to get the mode parameter
    if (idx == -1) {
        response->println("Usage: KEYS MODE <DUTY|FREQ>");
        return;
    }

    String mode = cmd.substring(idx + 1);
    mode.trim();
    mode.toUpperCase();

    if (mode == "DUTY") {
        peripheralManager.setKeyControlMode(true);
        response->println("Key control mode: Duty adjustment");
    } else if (mode == "FREQ" || mode == "FREQUENCY") {
        peripheralManager.setKeyControlMode(false);
        response->println("Key control mode: Frequency adjustment");
    } else {
        response->println("ERROR: Invalid mode. Use DUTY or FREQ");
    }
}

// ============================================================================
// Peripheral Status Commands
// ============================================================================

void CommandParser::handlePeripheralStatus(ICommandResponse* response) {
    response->println("Peripheral Status Summary:");
    response->printf("  UART1: %s\n", peripheralManager.getUART1().getModeName());
    response->printf("  UART2: %u baud\n", peripheralManager.getUART2().getBaudRate());
    response->printf("  Buzzer: %s (%u Hz)\n",
                    peripheralManager.getBuzzer().isEnabled() ? "ON" : "OFF",
                    peripheralManager.getBuzzer().getFrequency());
    response->printf("  LED PWM: %s (%.1f%%)\n",
                    peripheralManager.getLEDPWM().isEnabled() ? "ON" : "OFF",
                    peripheralManager.getLEDPWM().getBrightness());
    response->printf("  Relay: %s\n",
                    peripheralManager.getRelay().getState() ? "ON" : "OFF");
    response->printf("  GPIO: %s\n",
                    peripheralManager.getGPIO().getState() ? "HIGH" : "LOW");
    response->printf("  Keys: %s\n",
                    peripheralManager.isKeyControlEnabled() ? "Enabled" : "Disabled");
}

void CommandParser::handlePeripheralStats(ICommandResponse* response) {
    String stats = peripheralManager.getStatistics();
    response->print(stats.c_str());
}

// ============================================================================
// Peripheral Settings Commands
// ============================================================================

void CommandParser::handlePeripheralSave(ICommandResponse* response) {
    if (peripheralManager.saveSettings()) {
        response->println("OK: Peripheral settings saved to NVS");
    } else {
        response->println("ERROR: Failed to save peripheral settings");
    }
}

void CommandParser::handlePeripheralLoad(ICommandResponse* response) {
    if (peripheralManager.loadSettings()) {
        response->println("OK: Peripheral settings loaded from NVS");
        if (peripheralManager.applySettings()) {
            response->println("OK: Settings applied to all peripherals");
        } else {
            response->println("WARNING: Some settings may not have been applied");
        }
    } else {
        response->println("ERROR: Failed to load peripheral settings");
    }
}

void CommandParser::handlePeripheralReset(ICommandResponse* response) {
    peripheralManager.resetSettings();
    response->println("OK: Peripheral settings reset to defaults");
    response->println("INFO: Use 'PERIPHERAL LOAD' to apply default settings");
}
