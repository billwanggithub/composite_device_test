#include "WebServer.h"
#include "ArduinoJson.h"

// ============================================================================
// Peripheral API Handlers
// ============================================================================

void WebServerManager::handleGetPeripheralStatus(AsyncWebServerRequest *request) {
    if (!pPeripheralManager) {
        request->send(503, "application/json", "{\"error\":\"Peripheral manager not available\"}");
        return;
    }

    StaticJsonDocument<1024> doc;

    // UART1 Status
    JsonObject uart1 = doc.createNestedObject("uart1");
    uart1["mode"] = pPeripheralManager->getUART1().getModeName();
    if (pPeripheralManager->getUART1().getMode() == UART1Mux::MODE_PWM_RPM) {
        uart1["pwm_freq"] = pPeripheralManager->getUART1().getPWMFrequency();
        uart1["pwm_duty"] = pPeripheralManager->getUART1().getPWMDuty();
        uart1["rpm_freq"] = pPeripheralManager->getUART1().getRPMFrequency();
        uart1["pwm_enabled"] = pPeripheralManager->getUART1().isPWMEnabled();
    } else if (pPeripheralManager->getUART1().getMode() == UART1Mux::MODE_UART) {
        uart1["baud"] = pPeripheralManager->getUART1().getUARTBaudRate();
    }

    // UART2 Status
    JsonObject uart2 = doc.createNestedObject("uart2");
    uart2["baud"] = pPeripheralManager->getUART2().getBaudRate();

    // Buzzer Status
    JsonObject buzzer = doc.createNestedObject("buzzer");
    buzzer["enabled"] = pPeripheralManager->getBuzzer().isEnabled();
    buzzer["frequency"] = pPeripheralManager->getBuzzer().getFrequency();
    buzzer["duty"] = pPeripheralManager->getBuzzer().getDuty();

    // LED PWM Status
    JsonObject led = doc.createNestedObject("led");
    led["enabled"] = pPeripheralManager->getLEDPWM().isEnabled();
    led["frequency"] = pPeripheralManager->getLEDPWM().getFrequency();
    led["brightness"] = pPeripheralManager->getLEDPWM().getBrightness();

    // Relay Status
    JsonObject relay = doc.createNestedObject("relay");
    relay["state"] = pPeripheralManager->getRelay().getState();

    // GPIO Status
    JsonObject gpio = doc.createNestedObject("gpio");
    gpio["state"] = pPeripheralManager->getGPIO().getState();

    // User Keys Status
    JsonObject keys = doc.createNestedObject("keys");
    keys["key1"] = pPeripheralManager->getKeys().isPressed(UserKeys::KEY1);
    keys["key2"] = pPeripheralManager->getKeys().isPressed(UserKeys::KEY2);
    keys["key3"] = pPeripheralManager->getKeys().isPressed(UserKeys::KEY3);
    keys["mode"] = pPeripheralManager->isKeyControlAdjustingDuty() ? "duty" : "frequency";
    keys["control_enabled"] = pPeripheralManager->isKeyControlEnabled();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleGetUART1Status(AsyncWebServerRequest *request) {
    if (!pPeripheralManager) {
        request->send(503, "application/json", "{\"error\":\"Peripheral manager not available\"}");
        return;
    }

    StaticJsonDocument<512> doc;
    doc["mode"] = pPeripheralManager->getUART1().getModeName();

    if (pPeripheralManager->getUART1().getMode() == UART1Mux::MODE_PWM_RPM) {
        doc["pwm_freq"] = pPeripheralManager->getUART1().getPWMFrequency();
        doc["pwm_duty"] = pPeripheralManager->getUART1().getPWMDuty();
        doc["rpm_freq"] = pPeripheralManager->getUART1().getRPMFrequency();
        doc["pwm_enabled"] = pPeripheralManager->getUART1().isPWMEnabled();
    } else if (pPeripheralManager->getUART1().getMode() == UART1Mux::MODE_UART) {
        doc["baud"] = pPeripheralManager->getUART1().getUARTBaudRate();
        uint32_t tx, rx, err;
        pPeripheralManager->getUART1().getUARTStatistics(&tx, &rx, &err);
        doc["tx_bytes"] = tx;
        doc["rx_bytes"] = rx;
        doc["errors"] = err;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handlePostUART1Mode(AsyncWebServerRequest *request) {
    if (!pPeripheralManager) {
        request->send(503, "application/json", "{\"error\":\"Peripheral manager not available\"}");
        return;
    }

    if (!request->hasParam("mode", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing 'mode' parameter\"}");
        return;
    }

    String mode = request->getParam("mode", true)->value();
    mode.toUpperCase();

    bool success = false;
    if (mode == "UART") {
        uint32_t baud = request->hasParam("baud", true) ?
                        request->getParam("baud", true)->value().toInt() : 115200;
        success = pPeripheralManager->getUART1().setModeUART(baud);
    } else if (mode == "PWM") {
        success = pPeripheralManager->getUART1().setModePWM_RPM();
    } else if (mode == "DISABLED") {
        pPeripheralManager->getUART1().disable();
        success = true;
    } else {
        request->send(400, "application/json", "{\"error\":\"Invalid mode. Use UART, PWM, or DISABLED\"}");
        return;
    }

    if (success) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Mode change failed\"}");
    }
}

void WebServerManager::handlePostUART1PWM(AsyncWebServerRequest *request) {
    if (!pPeripheralManager) {
        request->send(503, "application/json", "{\"error\":\"Peripheral manager not available\"}");
        return;
    }

    if (pPeripheralManager->getUART1().getMode() != UART1Mux::MODE_PWM_RPM) {
        request->send(400, "application/json", "{\"error\":\"UART1 not in PWM mode\"}");
        return;
    }

    bool success = true;
    String message = "PWM updated";

    if (request->hasParam("frequency", true)) {
        uint32_t freq = request->getParam("frequency", true)->value().toInt();
        if (!pPeripheralManager->getUART1().setPWMFrequency(freq)) {
            success = false;
            message = "Invalid frequency";
        }
    }

    if (success && request->hasParam("duty", true)) {
        float duty = request->getParam("duty", true)->value().toFloat();
        if (!pPeripheralManager->getUART1().setPWMDuty(duty)) {
            success = false;
            message = "Invalid duty cycle";
        }
    }

    if (success && request->hasParam("enabled", true)) {
        bool enabled = request->getParam("enabled", true)->value() == "true";
        pPeripheralManager->getUART1().setPWMEnabled(enabled);
    }

    StaticJsonDocument<128> doc;
    doc["success"] = success;
    if (!success) {
        doc["error"] = message;
    }

    String response;
    serializeJson(doc, response);
    request->send(success ? 200 : 400, "application/json", response);
}

void WebServerManager::handleGetUART2Status(AsyncWebServerRequest *request) {
    if (!pPeripheralManager) {
        request->send(503, "application/json", "{\"error\":\"Peripheral manager not available\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    doc["baud"] = pPeripheralManager->getUART2().getBaudRate();

    uint32_t tx, rx, err;
    pPeripheralManager->getUART2().getStatistics(&tx, &rx, &err);
    doc["tx_bytes"] = tx;
    doc["rx_bytes"] = rx;
    doc["errors"] = err;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handlePostBuzzer(AsyncWebServerRequest *request) {
    if (!pPeripheralManager) {
        request->send(503, "application/json", "{\"error\":\"Peripheral manager not available\"}");
        return;
    }

    bool success = true;
    String message = "Buzzer updated";

    if (request->hasParam("frequency", true)) {
        uint32_t freq = request->getParam("frequency", true)->value().toInt();
        if (!pPeripheralManager->getBuzzer().setFrequency(freq)) {
            success = false;
            message = "Invalid frequency (10-20000 Hz)";
        }
    }

    if (success && request->hasParam("duty", true)) {
        float duty = request->getParam("duty", true)->value().toFloat();
        if (!pPeripheralManager->getBuzzer().setDuty(duty)) {
            success = false;
            message = "Invalid duty cycle (0-100%)";
        }
    }

    if (success && request->hasParam("enabled", true)) {
        bool enabled = request->getParam("enabled", true)->value() == "true";
        pPeripheralManager->getBuzzer().enable(enabled);
    }

    // Handle beep command
    if (success && request->hasParam("beep", true)) {
        uint32_t freq = request->hasParam("beep_freq", true) ?
                        request->getParam("beep_freq", true)->value().toInt() : 2000;
        uint32_t duration = request->hasParam("beep_duration", true) ?
                            request->getParam("beep_duration", true)->value().toInt() : 100;
        float duty = request->hasParam("beep_duty", true) ?
                     request->getParam("beep_duty", true)->value().toFloat() : 50.0;
        pPeripheralManager->getBuzzer().beep(freq, duration, duty);
        message = "Beep executed";
    }

    StaticJsonDocument<128> doc;
    doc["success"] = success;
    doc["message"] = message;
    if (!success) {
        doc["error"] = message;
    }

    String response;
    serializeJson(doc, response);
    request->send(success ? 200 : 400, "application/json", response);
}

void WebServerManager::handlePostLEDPWM(AsyncWebServerRequest *request) {
    if (!pPeripheralManager) {
        request->send(503, "application/json", "{\"error\":\"Peripheral manager not available\"}");
        return;
    }

    bool success = true;
    String message = "LED updated";

    if (request->hasParam("frequency", true)) {
        uint32_t freq = request->getParam("frequency", true)->value().toInt();
        if (!pPeripheralManager->getLEDPWM().setFrequency(freq)) {
            success = false;
            message = "Invalid frequency (100-20000 Hz)";
        }
    }

    if (success && request->hasParam("brightness", true)) {
        float brightness = request->getParam("brightness", true)->value().toFloat();
        if (!pPeripheralManager->getLEDPWM().setBrightness(brightness)) {
            success = false;
            message = "Invalid brightness (0-100%)";
        }
    }

    if (success && request->hasParam("enabled", true)) {
        bool enabled = request->getParam("enabled", true)->value() == "true";
        pPeripheralManager->getLEDPWM().enable(enabled);
    }

    StaticJsonDocument<128> doc;
    doc["success"] = success;
    doc["message"] = message;
    if (!success) {
        doc["error"] = message;
    }

    String response;
    serializeJson(doc, response);
    request->send(success ? 200 : 400, "application/json", response);
}

void WebServerManager::handlePostRelay(AsyncWebServerRequest *request) {
    if (!pPeripheralManager) {
        request->send(503, "application/json", "{\"error\":\"Peripheral manager not available\"}");
        return;
    }

    if (!request->hasParam("state", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing 'state' parameter\"}");
        return;
    }

    String state = request->getParam("state", true)->value();
    state.toLowerCase();

    if (state == "on" || state == "true" || state == "1") {
        pPeripheralManager->getRelay().setState(true);
    } else if (state == "off" || state == "false" || state == "0") {
        pPeripheralManager->getRelay().setState(false);
    } else if (state == "toggle") {
        pPeripheralManager->getRelay().toggle();
    } else {
        request->send(400, "application/json", "{\"error\":\"Invalid state. Use 'on', 'off', or 'toggle'\"}");
        return;
    }

    StaticJsonDocument<128> doc;
    doc["success"] = true;
    doc["state"] = pPeripheralManager->getRelay().getState();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handlePostGPIO(AsyncWebServerRequest *request) {
    if (!pPeripheralManager) {
        request->send(503, "application/json", "{\"error\":\"Peripheral manager not available\"}");
        return;
    }

    if (!request->hasParam("state", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing 'state' parameter\"}");
        return;
    }

    String state = request->getParam("state", true)->value();
    state.toLowerCase();

    if (state == "high" || state == "true" || state == "1") {
        pPeripheralManager->getGPIO().setHigh();
    } else if (state == "low" || state == "false" || state == "0") {
        pPeripheralManager->getGPIO().setLow();
    } else if (state == "toggle") {
        pPeripheralManager->getGPIO().toggle();
    } else {
        request->send(400, "application/json", "{\"error\":\"Invalid state. Use 'high', 'low', or 'toggle'\"}");
        return;
    }

    StaticJsonDocument<128> doc;
    doc["success"] = true;
    doc["state"] = pPeripheralManager->getGPIO().getState();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleGetKeys(AsyncWebServerRequest *request) {
    if (!pPeripheralManager) {
        request->send(503, "application/json", "{\"error\":\"Peripheral manager not available\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    doc["key1_pressed"] = pPeripheralManager->getKeys().isPressed(UserKeys::KEY1);
    doc["key2_pressed"] = pPeripheralManager->getKeys().isPressed(UserKeys::KEY2);
    doc["key3_pressed"] = pPeripheralManager->getKeys().isPressed(UserKeys::KEY3);
    doc["key1_state"] = pPeripheralManager->getKeys().getKeyStateName(UserKeys::KEY1);
    doc["key2_state"] = pPeripheralManager->getKeys().getKeyStateName(UserKeys::KEY2);
    doc["key3_state"] = pPeripheralManager->getKeys().getKeyStateName(UserKeys::KEY3);
    doc["control_mode"] = pPeripheralManager->isKeyControlAdjustingDuty() ? "duty" : "frequency";
    doc["control_enabled"] = pPeripheralManager->isKeyControlEnabled();
    doc["duty_step"] = pPeripheralManager->getDutyStep();
    doc["freq_step"] = pPeripheralManager->getFrequencyStep();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}
