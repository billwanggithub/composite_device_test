# Status LED Guide - WS2812 RGB LED Indicator

**Date:** 2025-11-06
**Hardware:** WS2812 RGB LED on GPIO 48
**Library:** Adafruit NeoPixel v1.12.0

---

## Overview

The status LED provides visual feedback for system state using a single WS2812 RGB LED (NeoPixel). The LED automatically changes color based on motor control state, BLE connection, and system errors.

---

## Hardware Setup

### Pin Connection

```
ESP32-S3 GPIO 48 ─── 470Ω resistor ─── WS2812 DIN
3.3V ───────────────────────────────── WS2812 VCC
GND ────────────────────────────────── WS2812 GND
```

**Recommended:**
- 470Ω resistor on data line (reduces signal ringing)
- 100µF capacitor between VCC and GND (power stability)
- Keep wire length < 15 cm for reliable operation

### Supported LED Types

- WS2812 / WS2812B
- NeoPixel (Adafruit branded)
- Any WS281x compatible LED

---

## LED Color Codes

| Color | State | Meaning |
|-------|-------|---------|
| **Green** (Solid) | System Ready | Motor idle, no errors |
| **Blue** (Solid) | Motor Running | PWM duty > 0%, motor active |
| **Purple** (Solid) | BLE Connected | BLE client connected |
| **Yellow** (Blinking, 200ms) | Initializing | System starting up |
| **Red** (Blinking, 500ms) | Error/Emergency | Safety alert, emergency stop |
| **Off** | Not Initialized | System not started |

---

## LED Control Commands

### Set LED Brightness

```
SET LED_BRIGHTNESS <0-255>
```

**Examples:**
```
> SET LED_BRIGHTNESS 50
✅ LED 亮度設定為: 50 (已立即套用)
ℹ️ 使用 SAVE 命令儲存設定

> SET LED_BRIGHTNESS 255
✅ LED 亮度設定為: 255 (已立即套用)

> SET LED_BRIGHTNESS 0
✅ LED 亮度設定為: 0 (已立即套用)
(LED appears off but still updating)
```

**Brightness Levels:**
- `0` - Off (LED still updates, just very dim)
- `25` - Default (good for indoor use)
- `50` - Medium (balanced visibility)
- `100` - Bright (good visibility)
- `255` - Maximum (very bright, may be uncomfortable)

**Note:** Brightness changes take effect immediately and are saved to NVS with the SAVE command.

---

## Automatic State Indication

The LED automatically changes based on system state (updated every 1 second):

### Priority Order (highest to lowest)

1. **Emergency Stop** (Red Blinking)
   - Triggered by safety check failure
   - Overspeed or critical error
   - Manual emergency stop command

2. **BLE Connected** (Purple Solid)
   - BLE client connected to device
   - Overrides motor state indication

3. **Motor Running** (Blue Solid)
   - PWM duty cycle > 0.1%
   - Motor actively controlled

4. **System Ready** (Green Solid)
   - Motor idle (duty = 0%)
   - System healthy and ready

5. **Initializing** (Yellow Blinking)
   - Only during system startup
   - Changes to Green when complete

---

## Programming Interface

### StatusLED Class Methods

```cpp
// Initialization
statusLED.begin(pin, brightness);  // GPIO 48, brightness 0-255

// Solid colors
statusLED.setColor(r, g, b);      // Custom RGB color
statusLED.setGreen();             // Solid green
statusLED.setBlue();              // Solid blue
statusLED.setRed();               // Solid red
statusLED.setYellow();            // Solid yellow
statusLED.setPurple();            // Solid purple
statusLED.setCyan();              // Solid cyan
statusLED.setWhite();             // Solid white
statusLED.off();                  // Turn off

// Blinking colors
statusLED.setBlink(r, g, b, intervalMs);  // Custom blinking
statusLED.blinkRed(500);                  // Blink red at 500ms
statusLED.blinkYellow(200);               // Blink yellow at 200ms
statusLED.blinkGreen(1000);               // Blink green at 1000ms

// Brightness control
statusLED.setBrightness(brightness);  // Set 0-255
uint8_t current = statusLED.getBrightness();

// Update (must call in loop)
statusLED.update();  // Updates blink timing

// Status check
bool ready = statusLED.isInitialized();
```

### Example Usage

```cpp
// In setup()
statusLED.begin(48, 25);  // GPIO 48, brightness 25
statusLED.blinkYellow(200);  // Show initialization

// After setup complete
statusLED.setGreen();  // System ready

// In motor task
if (motor.getPWMDuty() > 0) {
    statusLED.setBlue();  // Motor running
} else {
    statusLED.setGreen();  // Motor idle
}

// In main loop()
statusLED.update();  // Update blink timing (required!)
```

---

## Troubleshooting

### LED Not Working

**Symptoms:** LED remains off or shows wrong colors

**Checks:**
1. Verify power connection (VCC = 3.3V, GND connected)
2. Check data line connection (GPIO 48)
3. Verify LED type (WS2812/WS281x)
4. Check orientation (DIN on correct pin)
5. Test with known working LED

**Solutions:**
```
> SET LED_BRIGHTNESS 255
(Should make LED very bright if working)

Check serial output:
✅ Status LED initialized (GPIO 48, brightness 25)

If not initialized:
⚠️ Status LED initialization failed!
(Check wiring and power)
```

### LED Flickering or Glitching

**Possible Causes:**
- Insufficient power supply
- Data line too long (> 15 cm)
- Missing 470Ω resistor
- Electrical noise

**Solutions:**
1. Add 470Ω resistor on data line
2. Add 100µF capacitor near LED
3. Shorten data wire
4. Use shielded cable for long runs
5. Ensure stable 3.3V power supply

### LED Shows Wrong Color

**Causes:**
- LED type mismatch (RGB vs GRB ordering)
- Damaged LED
- Voltage level issues

**Solutions:**
1. Try different LED from same batch
2. Verify LED is WS2812 (GRB) compatible
3. Check voltage levels with multimeter

### LED Brightness Too Low

```
> SET LED_BRIGHTNESS 255
(Maximum brightness)

> STATUS
(Check current brightness setting)
```

### LED Doesn't Blink

**Symptom:** Blinking mode not working

**Causes:**
- `statusLED.update()` not called in loop()
- Loop delay too long

**Solution:**
```cpp
void loop() {
    statusLED.update();  // REQUIRED for blinking!
    vTaskDelay(pdMS_TO_TICKS(50));  // Keep < 100ms for smooth blink
}
```

---

## Performance Characteristics

### Update Timing
- **Update Rate:** 50 ms (20 Hz) in main loop
- **Blink Precision:** ±50 ms (loop interval dependent)
- **Blink Range:** 100 ms - 5000 ms (validated)
- **Color Change Time:** < 1 ms (instant)

### Power Consumption
- **Off:** ~0 mA
- **Brightness 25:** ~5 mA
- **Brightness 128:** ~20 mA
- **Brightness 255 (white):** ~60 mA maximum

### Memory Usage
- **Flash:** ~2 KB (NeoPixel library)
- **RAM:** ~100 bytes (StatusLED instance)
- **No heap allocation** after initialization

---

## Advanced Features

### Custom LED Patterns

You can create custom patterns by controlling the LED directly:

```cpp
// Rainbow fade effect
for(int i=0; i<256; i++) {
    uint8_t r = (i < 85) ? 255 - i*3 : 0;
    uint8_t g = (i < 85) ? i*3 : (i < 170) ? 255 - (i-85)*3 : 0;
    uint8_t b = (i >= 170) ? (i-170)*3 : 0;
    statusLED.setColor(r, g, b);
    delay(10);
}

// Breathing effect
for(int brightness=0; brightness<=255; brightness+=5) {
    statusLED.setBrightness(brightness);
    delay(20);
}
for(int brightness=255; brightness>=0; brightness-=5) {
    statusLED.setBrightness(brightness);
    delay(20);
}
```

### Multiple LED Support

To control multiple LEDs, modify StatusLED.h:

```cpp
// In StatusLED constructor
pixel = new Adafruit_NeoPixel(3, ledPin, NEO_GRB + NEO_KHZ800);
                            // ↑ Change to number of LEDs
```

### Color Calibration

If colors appear incorrect, you can adjust in StatusLED.h:

```cpp
namespace LEDColors {
    // Adjust color values if LED shows wrong colors
    constexpr uint8_t RED_R = 255, RED_G = 0, RED_B = 0;
    // For some LEDs, red might need:
    // constexpr uint8_t RED_R = 200, RED_G = 0, RED_B = 0;
}
```

---

## LED State Machine

```
System Boot
    ↓
[Yellow Blinking] ← Initialization
    ↓
[Green Solid] ← System Ready / Motor Idle
    ↓
    ├─→ [Blue Solid] ← Motor Running (duty > 0%)
    ├─→ [Purple Solid] ← BLE Connected
    └─→ [Red Blinking] ← Emergency Stop / Error
```

---

## Future Enhancements

Possible additions (not currently implemented):

1. **WiFi Indicator** - Cyan color for WiFi connected
2. **RPM-Based Colors** - Color gradient based on RPM
3. **Multiple LED Support** - LED strip animations
4. **Brightness Auto-Adjust** - Based on ambient light
5. **Custom Patterns** - User-programmable LED sequences
6. **Error Codes** - Blink patterns for specific errors

---

## Configuration Defaults

```cpp
// Default values (can be changed via commands)
#define DEFAULT_LED_PIN 48
#define DEFAULT_LED_BRIGHTNESS 25
#define DEFAULT_BLINK_INTERVAL 500  // milliseconds

// Color codes
LEDColors::GREEN   = (0, 255, 0)
LEDColors::BLUE    = (0, 0, 255)
LEDColors::RED     = (255, 0, 0)
LEDColors::YELLOW  = (255, 255, 0)
LEDColors::PURPLE  = (128, 0, 128)
LEDColors::CYAN    = (0, 255, 255)
```

---

## References

- **Adafruit NeoPixel Library:** https://github.com/adafruit/Adafruit_NeoPixel
- **WS2812 Datasheet:** https://www.adafruit.com/product/1655
- **ESP32-S3 GPIO Reference:** ESP32-S3 Technical Reference Manual

---

**Document Version:** 1.0
**Last Updated:** 2025-11-06
**Compatible Firmware:** v2.4.0+
