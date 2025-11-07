# Status LED Guide - WS2812 RGB LED Indicator

**Date:** 2025-11-07
**Hardware:** WS2812 RGB LED on GPIO 48
**Library:** Adafruit NeoPixel v1.12.0

---

## Overview

The status LED provides visual feedback for system state using a single WS2812 RGB LED (NeoPixel). The LED automatically changes color based on motor control state, BLE connection, web server status, and system errors.

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

| Color | State | Blink Rate | Meaning |
|-------|-------|-----------|---------|
| **Red** (Blinking) | CRITICAL ERROR | **100ms (10 Hz)** | Emergency stop, safety alert, initialization failure |
| **Yellow** (Blinking) | Web Server Not Ready | 500ms (2 Hz) | WiFi connected but web server not started |
| **Yellow** (Blinking) | Initializing | 200ms (5 Hz) | System starting up, BLE/WiFi initializing |
| **Purple** (Solid) | BLE Connected | - | BLE client connected (highest normal priority) |
| **Yellow** (Solid) | Ramping | - | Motor frequency/duty ramping in progress |
| **Blue** (Solid) | Motor Running | - | PWM duty > 0.1%, motor active |
| **Green** (Solid) | System Ready | - | Motor idle, no errors, web server running |
| **Off** | Not Initialized | - | System not started |

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

The LED automatically changes based on system state (updated every 200ms for fast response):

### Priority Order (highest to lowest)

1. **CRITICAL ERROR** (Red Blinking, **100ms**)
   - **HIGHEST PRIORITY** - Overrides all other states
   - **LATCHED ALARM** - Persists until explicitly cleared
   - Emergency stop activated (safety check failure)
   - Watchdog timeout detected
   - Motor control initialization failure
   - FreeRTOS resource creation failure
   - **Very fast blinking (10 Hz)** for immediate attention
   - **Must clear with:** `CLEAR ERROR` or `RESUME` command

2. **Web Server Not Ready** (Yellow Blinking, 500ms)
   - WiFi connected but web server not started
   - Indicates system waiting for web server
   - Helps identify web server startup issues

3. **BLE Connected** (Purple Solid)
   - BLE client connected to device
   - Overrides motor state indication

4. **Motor Ramping** (Yellow Solid)
   - Frequency or duty cycle ramping in progress
   - Smooth transition between values

5. **Motor Running** (Blue Solid)
   - PWM duty cycle > 0.1%
   - Motor actively controlled

6. **System Ready** (Green Solid)
   - Motor idle (duty = 0%)
   - Web server running
   - System healthy and ready

7. **Initializing** (Yellow Blinking, 200ms)
   - Only during system startup (setup() function)
   - Changes to normal state when motorTask starts

---

## Initialization LED Visibility

**Important:** The LED now properly blinks during the entire boot process!

During `setup()` initialization, `statusLED.update()` is called at strategic points to ensure the LED blinks visibly:

### Initialization Phases with LED Feedback

1. **USB Serial Wait** (up to 5 seconds)
   - LED: Yellow blinking (200ms)
   - Updates every 100ms during wait loop

2. **WiFi Initialization**
   - LED: Yellow blinking (200ms)
   - Updates before/after WiFi start
   - Updates before web server start

3. **BLE Initialization**
   - LED: Yellow blinking (200ms)
   - Updates during BLE device init
   - Updates after service start
   - Updates after advertising start

4. **Task Creation**
   - LED: Yellow blinking (200ms)
   - Updates before creating FreeRTOS tasks

**After `setup()` completes:**
- `motorTask` takes over LED control
- LED updates every 200ms based on system state
- `loop()` continues calling `update()` for blink timing

This ensures you can **visually monitor** the boot process and identify where initialization might be stuck.

---

## Latched Alarm Behavior (Safety-Critical)

**Important:** Critical error LED follows a latched alarm pattern for safety:

### How It Works

1. **Error Triggers** → Fast red LED (100ms) starts immediately
2. **Condition Clears** → Motor stops, safety OK, but **LED keeps blinking**
3. **Manual Clear Required** → User must explicitly acknowledge error
4. **LED Resumes Normal** → Only after clear command

### Why Latched Alarms?

This is a **safety-critical system best practice**:
- ✅ **Cannot be missed** - LED continues even after auto-recovery
- ✅ **Requires acknowledgment** - User must actively respond
- ✅ **Audit trail** - Error and clearance logged to serial
- ✅ **Prevents accidents** - No silent auto-reset

### Clear Commands

To clear the emergency stop and resume normal operation:

```bash
# Any of these commands work:
CLEAR ERROR
CLEAR_ERROR
RESUME

# Response:
✅ 錯誤已清除 - 系統已恢復正常
Emergency error cleared - System resumed

# LED returns to normal state
```

### Example Scenario

```
1. Motor running at high speed
2. Overspeed detected (RPM > max)
   → Serial: "⚠️ SAFETY ALERT: Emergency stop activated!"
   → LED: Fast red blink (100ms) starts
   → Motor: Stops immediately
3. RPM drops to zero (condition cleared)
   → LED: Still blinking red (latched!)
4. User types: RESUME
   → Serial: "✅ Emergency stop cleared"
   → LED: Returns to green (system ready)
```

**Note:** Emergency stop flag persists across the entire system - all interfaces (CDC, HID, BLE, Web) will see the error state until explicitly cleared.

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
- **Update Rate:** 50 ms (20 Hz) in main loop for blink timing
- **State Update Rate:** 200 ms (5 Hz) in motorTask for state changes
- **Blink Precision:** ±50 ms (loop interval dependent)
- **Blink Range:** 100 ms - 5000 ms (validated)
- **Critical Error Blink:** 100 ms (10 Hz, very fast for urgent attention)
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
[Yellow Blinking, 200ms] ← Initialization (setup() function)
    ↓                       (Visible during WiFi/BLE/Web server init)
    ├─→ [Red Blinking, 100ms]** ← CRITICAL ERROR (Motor init fail, etc.)
    │                               **HIGHEST PRIORITY - OVERRIDES ALL**
    ↓
[Yellow Blinking, 500ms] ← Web Server Not Ready (WiFi connected, web server starting)
    ↓
[Green Solid] ← System Ready / Motor Idle (web server running)
    ↓
    ├─→ [Purple Solid] ← BLE Connected (overrides motor state)
    ├─→ [Yellow Solid] ← Motor Ramping (smooth transitions)
    ├─→ [Blue Solid] ← Motor Running (duty > 0.1%)
    └─→ [Red Blinking, 100ms]** ← Emergency Stop / Safety Alert
                                    **FAST BLINK - URGENT!**
```

**Note:** Red fast blink (100ms) always has highest priority and will override any other state when critical errors occur.

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

**Document Version:** 2.1
**Last Updated:** 2025-11-07
**Compatible Firmware:** v2.6.0+

**Version 2.1 Changes:**
- Added latched alarm behavior section (safety-critical)
- Documented CLEAR ERROR / RESUME commands
- Added emergency stop clearance workflow
- Explained why errors persist after condition clears

**Version 2.0 Changes:**
- Added fast red blink (100ms) for critical errors
- Added web server not ready indicator (yellow 500ms blink)
- Updated LED update rate to 200ms for faster response
- Added LED visibility during setup() initialization
- Updated priority system with error state override
