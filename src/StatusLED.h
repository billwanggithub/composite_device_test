#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

/**
 * @brief Status LED controller for WS2812 RGB LED
 *
 * Provides visual feedback for system status using a single WS2812 RGB LED.
 * Supports solid colors, blinking, and brightness control.
 *
 * Color Codes:
 * - Green (Solid):      System ready, motor idle
 * - Blue (Solid):       Motor running normally
 * - Yellow (Blink):     Initialization in progress OR Web server not ready
 * - Red (Fast Blink):   CRITICAL ERROR - Emergency stop, Safety alert, Initialization failure (100ms)
 * - Purple (Solid):     BLE connected
 * - Cyan (Solid):       WiFi connected (future)
 * - Off:                System not initialized
 *
 * Hardware:
 * - WS2812 RGB LED on GPIO 48
 * - 470Ω resistor recommended on data line
 *
 * Usage:
 *   StatusLED led;
 *   led.begin(48, 25);  // GPIO 48, brightness 25
 *   led.setColor(0, 255, 0);  // Green
 *   led.setBlink(255, 0, 0, 500);  // Red blinking at 500ms
 *   led.update();  // Call in main loop
 */
class StatusLED {
public:
    /**
     * @brief Constructor
     */
    StatusLED();

    /**
     * @brief Destructor
     */
    ~StatusLED();

    /**
     * @brief Initialize the LED
     * @param pin GPIO pin number (default: 48)
     * @param brightness Initial brightness 0-255 (default: 25)
     * @return true if initialization successful
     */
    bool begin(int pin = 48, uint8_t brightness = 25);

    /**
     * @brief Set LED to solid color
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     *
     * Disables blinking and sets solid color
     */
    void setColor(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Set LED to blinking mode
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @param intervalMs Blink interval in milliseconds (100-5000)
     *
     * LED will toggle between color and off at specified interval
     */
    void setBlink(uint8_t r, uint8_t g, uint8_t b, uint32_t intervalMs = 500);

    /**
     * @brief Set brightness level
     * @param brightness Brightness 0-255 (0=off, 255=max)
     */
    void setBrightness(uint8_t brightness);

    /**
     * @brief Get current brightness level
     * @return Current brightness (0-255)
     */
    uint8_t getBrightness() const;

    /**
     * @brief Turn LED off
     */
    void off();

    /**
     * @brief Update LED state (must be called regularly in loop)
     *
     * Call this function in main loop to update blink timing
     * Recommended: Call every 10-50ms
     */
    void update();

    /**
     * @brief Check if LED is initialized
     * @return true if begin() was called successfully
     */
    bool isInitialized() const;

    // Predefined color methods for common states
    void setGreen();      // System ready
    void setBlue();       // Motor running
    void setRed();        // Error
    void setYellow();     // Warning
    void setPurple();     // BLE connected
    void setCyan();       // WiFi connected
    void setWhite();      // Test/Calibration

    // Predefined blinking states
    void blinkRed(uint32_t intervalMs = 500);     // Error blink
    void blinkYellow(uint32_t intervalMs = 200);  // Init blink
    void blinkGreen(uint32_t intervalMs = 1000);  // Success blink

private:
    Adafruit_NeoPixel* pixel = nullptr;
    int ledPin = -1;
    uint8_t brightness = 25;

    // Current color state
    uint8_t currentR = 0;
    uint8_t currentG = 0;
    uint8_t currentB = 0;

    // Blink state
    bool blinkEnabled = false;
    uint32_t blinkInterval = 500;
    uint32_t lastBlinkTime = 0;
    bool blinkState = false;  // true = on, false = off

    bool initialized = false;

    /**
     * @brief Apply current color to LED hardware
     */
    void applyColor();
};

// Predefined RGB color values
namespace LEDColors {
    // Basic colors
    constexpr uint8_t RED_R = 255, RED_G = 0, RED_B = 0;
    constexpr uint8_t GREEN_R = 0, GREEN_G = 255, GREEN_B = 0;
    constexpr uint8_t BLUE_R = 0, BLUE_G = 0, BLUE_B = 255;
    constexpr uint8_t YELLOW_R = 255, YELLOW_G = 255, YELLOW_B = 0;
    constexpr uint8_t PURPLE_R = 128, PURPLE_G = 0, PURPLE_B = 128;
    constexpr uint8_t CYAN_R = 0, CYAN_G = 255, CYAN_B = 255;
    constexpr uint8_t WHITE_R = 255, WHITE_G = 255, WHITE_B = 255;
    constexpr uint8_t OFF_R = 0, OFF_G = 0, OFF_B = 0;

    // Dimmed versions for lower brightness
    constexpr uint8_t DIM_RED_R = 128, DIM_RED_G = 0, DIM_RED_B = 0;
    constexpr uint8_t DIM_GREEN_R = 0, DIM_GREEN_G = 128, DIM_GREEN_B = 0;
    constexpr uint8_t DIM_BLUE_R = 0, DIM_BLUE_G = 0, DIM_BLUE_B = 128;
}

#endif // STATUS_LED_H
