#include "StatusLED.h"
#include "USBCDC.h"

// External reference to USBSerial (defined in main.cpp)
extern USBCDC USBSerial;


StatusLED::StatusLED() {
    // Constructor - initialization happens in begin()
}

StatusLED::~StatusLED() {
    if (pixel) {
        delete pixel;
        pixel = nullptr;
    }
}

bool StatusLED::begin(int pin, uint8_t initialBrightness) {
    if (initialized) {
        USBSerial.println("⚠️ StatusLED already initialized");
        return true;
    }

    ledPin = pin;
    brightness = initialBrightness;

    // Create NeoPixel object (1 LED, GRB format, 800 KHz)
    pixel = new Adafruit_NeoPixel(1, ledPin, NEO_GRB + NEO_KHZ800);

    if (!pixel) {
        USBSerial.println("❌ Failed to create NeoPixel object");
        return false;
    }

    // Initialize the pixel
    pixel->begin();
    pixel->setBrightness(brightness);
    pixel->show();  // Initialize all pixels to 'off'

    initialized = true;

    USBSerial.printf("✅ Status LED initialized (GPIO %d, brightness %d)\n", ledPin, brightness);
    return true;
}

void StatusLED::setColor(uint8_t r, uint8_t g, uint8_t b) {
    if (!initialized) return;

    currentR = r;
    currentG = g;
    currentB = b;
    blinkEnabled = false;
    blinkState = true;  // Always on for solid color

    applyColor();
}

void StatusLED::setBlink(uint8_t r, uint8_t g, uint8_t b, uint32_t intervalMs) {
    if (!initialized) return;

    // Validate blink interval
    if (intervalMs < 100) intervalMs = 100;
    if (intervalMs > 5000) intervalMs = 5000;

    currentR = r;
    currentG = g;
    currentB = b;
    blinkInterval = intervalMs;
    blinkEnabled = true;
    lastBlinkTime = millis();
    blinkState = true;  // Start with LED on

    applyColor();
}

void StatusLED::setBrightness(uint8_t newBrightness) {
    if (!initialized) return;

    brightness = newBrightness;
    pixel->setBrightness(brightness);

    // Re-apply current color with new brightness
    applyColor();
}

uint8_t StatusLED::getBrightness() const {
    return brightness;
}

void StatusLED::off() {
    if (!initialized) return;

    currentR = 0;
    currentG = 0;
    currentB = 0;
    blinkEnabled = false;
    blinkState = false;

    pixel->setPixelColor(0, pixel->Color(0, 0, 0));
    pixel->show();
}

void StatusLED::update() {
    if (!initialized || !blinkEnabled) return;

    unsigned long now = millis();

    // Check if it's time to toggle
    if (now - lastBlinkTime >= blinkInterval) {
        blinkState = !blinkState;
        lastBlinkTime = now;
        applyColor();
    }
}

bool StatusLED::isInitialized() const {
    return initialized;
}

void StatusLED::applyColor() {
    if (!initialized) return;

    if (blinkEnabled) {
        // Blink mode: toggle between color and off
        if (blinkState) {
            pixel->setPixelColor(0, pixel->Color(currentR, currentG, currentB));
        } else {
            pixel->setPixelColor(0, pixel->Color(0, 0, 0));
        }
    } else {
        // Solid mode: always show color
        pixel->setPixelColor(0, pixel->Color(currentR, currentG, currentB));
    }

    pixel->show();
}

// Predefined color methods
void StatusLED::setGreen() {
    setColor(LEDColors::GREEN_R, LEDColors::GREEN_G, LEDColors::GREEN_B);
}

void StatusLED::setBlue() {
    setColor(LEDColors::BLUE_R, LEDColors::BLUE_G, LEDColors::BLUE_B);
}

void StatusLED::setRed() {
    setColor(LEDColors::RED_R, LEDColors::RED_G, LEDColors::RED_B);
}

void StatusLED::setYellow() {
    setColor(LEDColors::YELLOW_R, LEDColors::YELLOW_G, LEDColors::YELLOW_B);
}

void StatusLED::setPurple() {
    setColor(LEDColors::PURPLE_R, LEDColors::PURPLE_G, LEDColors::PURPLE_B);
}

void StatusLED::setCyan() {
    setColor(LEDColors::CYAN_R, LEDColors::CYAN_G, LEDColors::CYAN_B);
}

void StatusLED::setWhite() {
    setColor(LEDColors::WHITE_R, LEDColors::WHITE_G, LEDColors::WHITE_B);
}

// Predefined blinking states
void StatusLED::blinkRed(uint32_t intervalMs) {
    setBlink(LEDColors::RED_R, LEDColors::RED_G, LEDColors::RED_B, intervalMs);
}

void StatusLED::blinkYellow(uint32_t intervalMs) {
    setBlink(LEDColors::YELLOW_R, LEDColors::YELLOW_G, LEDColors::YELLOW_B, intervalMs);
}

void StatusLED::blinkGreen(uint32_t intervalMs) {
    setBlink(LEDColors::GREEN_R, LEDColors::GREEN_G, LEDColors::GREEN_B, intervalMs);
}
