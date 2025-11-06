# Motor Control Integration Plan

**Date:** 2025-11-06
**Target:** ESP32-S3 Composite Device
**Goal:** Add PWM motor control and RPM measurement to existing USB/BLE console system

---

## Table of Contents

1. [Overview](#overview)
2. [Current System Analysis](#current-system-analysis)
3. [Integration Strategy](#integration-strategy)
4. [Architecture Design](#architecture-design)
5. [Implementation Phases](#implementation-phases)
6. [Testing Plan](#testing-plan)
7. [Future Enhancements](#future-enhancements)

---

## Overview

### Objective
Integrate motor control functionality into the existing ESP32-S3 composite device without breaking current USB CDC, USB HID, and BLE console features.

### Key Requirements
- **PWM Output**: High-precision motor control using MCPWM peripheral
- **Tachometer Input**: Hardware-based RPM measurement using MCPWM Capture
- **Multi-Interface Access**: Control via CDC, HID, and BLE
- **Settings Persistence**: Save/load configuration from NVS
- **Safety Features**: Overspeed protection, stall detection
- **Status Indication**: RGB LED for visual feedback

### Design Principles
- ✅ Preserve existing architecture
- ✅ Non-breaking changes
- ✅ Clean separation of concerns
- ✅ FreeRTOS best practices
- ✅ Thread-safe implementation

---

## Current System Analysis

### Existing Features
```
┌─────────────────────────────────────────┐
│  ESP32-S3 Composite Device (v2.2)       │
├─────────────────────────────────────────┤
│  Interfaces:                            │
│    - USB CDC Serial Console             │
│    - USB HID 64-byte Custom Protocol    │
│    - BLE GATT Command Interface         │
│                                         │
│  Architecture:                          │
│    - FreeRTOS Multi-tasking             │
│    - 3 Tasks: HID, CDC, BLE             │
│    - Unified Command Parser             │
│    - Response Routing System            │
│    - SCPI Command Support               │
└─────────────────────────────────────────┘
```

### Available Resources

**GPIO Pins:**
- GPIO 10: Available (PWM Output)
- GPIO 11: Available (Tachometer Input)
- GPIO 12: Available (Pulse Notification)
- GPIO 48: Available (RGB LED - WS2812)

**Peripherals:**
- MCPWM Unit 0: Available for Capture
- MCPWM Unit 1: Available for PWM
- NVS: Available for settings storage
- RMT: Available for WS2812 LED

**FreeRTOS Resources:**
- Core 1: All tasks currently run here
- Core 0: Available (used by Arduino WiFi/BLE stack)
- Stack memory: ~200KB free heap

---

## Integration Strategy

### Phase 1: Core Motor Control (NO WiFi)
Keep existing USB/BLE interfaces, add motor control features.

**Benefits:**
- Minimal changes to existing code
- Faster development
- Easier debugging
- Can add WiFi later if needed

### Phase 2: WiFi and Web Interface (Future)
Add WiFi connectivity and web server for remote control (optional).

---

## Architecture Design

### Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3 System                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │  USB CDC    │  │  USB HID    │  │   BLE GATT  │        │
│  │  Console    │  │  Protocol   │  │  Interface  │        │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘        │
│         │                │                 │                │
│         └────────────────┼─────────────────┘                │
│                          ▼                                  │
│              ┌───────────────────────┐                      │
│              │   Command Parser      │                      │
│              │  (Unified Interface)  │                      │
│              └───────────┬───────────┘                      │
│                          │                                  │
│         ┌────────────────┼────────────────┐                │
│         ▼                ▼                ▼                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   General   │  │    Motor    │  │  Settings   │        │
│  │  Commands   │  │   Control   │  │   Manager   │        │
│  └─────────────┘  └──────┬──────┘  └──────┬──────┘        │
│                           │                 │               │
│                           ▼                 ▼               │
│              ┌─────────────────────┐  ┌─────────┐          │
│              │   MotorControl      │  │   NVS   │          │
│              │   - PWM (MCPWM)     │  │ Storage │          │
│              │   - Tachometer      │  └─────────┘          │
│              │   - Safety Checks   │                       │
│              └─────────────────────┘                       │
│                                                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │  HID Task   │  │  CDC Task   │  │  BLE Task   │        │
│  │ Priority 2  │  │ Priority 1  │  │ Priority 1  │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
│                                                             │
│  ┌─────────────┐  ┌─────────────┐                         │
│  │ Motor Task  │  │  IDLE Task  │                         │
│  │ Priority 1  │  │ Priority 0  │                         │
│  └─────────────┘  └─────────────┘                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### File Structure

```
composite_device_test/
├── src/
│   ├── main.cpp                    # Update: Add motor initialization
│   ├── CommandParser.h/cpp         # Update: Add motor commands
│   ├── CustomHID.h/cpp            # Keep as-is
│   ├── HIDProtocol.h/cpp          # Keep as-is
│   ├── MotorControl.h              # NEW: Motor control interface
│   ├── MotorControl.cpp            # NEW: PWM + Tachometer impl
│   ├── MotorSettings.h             # NEW: Settings structure
│   ├── MotorSettings.cpp           # NEW: NVS persistence
│   ├── StatusLED.h                 # NEW: WS2812 LED control
│   └── StatusLED.cpp               # NEW: LED implementation
├── platformio.ini                  # Update: Add dependencies
├── CLAUDE.md                       # Existing: Project guide
├── PROTOCOL.md                     # Existing: HID protocol spec
├── TESTING.md                      # Existing: Testing guide
├── README.md                       # Update: Add motor features
└── MOTOR_INTEGRATION_PLAN.md       # This file
```

### New Classes

#### MotorControl Class
```cpp
class MotorControl {
public:
    // Initialization
    bool begin();
    void end();

    // PWM Control
    bool setPWMFrequency(uint32_t frequency);
    bool setPWMDuty(float duty);
    uint32_t getPWMFrequency() const;
    float getPWMDuty() const;

    // Tachometer & RPM
    void updateRPM();
    float getCurrentRPM() const;
    float getInputFrequency() const;

    // Safety & Status
    bool isInitialized() const;
    bool checkSafety();
    void emergencyStop();

    // Pulse Output (change notification)
    void sendPulse();

private:
    bool mcpwmInitialized = false;
    bool captureInitialized = false;
    uint32_t currentFrequency = 10000;
    float currentDuty = 0.0;
    float currentRPM = 0.0;
    float currentInputFrequency = 0.0;
    uint32_t lastCaptureTime = 0;

    // MCPWM Capture ISR callback
    static bool IRAM_ATTR captureCallback(mcpwm_unit_t mcpwm,
                                          mcpwm_capture_channel_id_t cap_channel,
                                          const cap_event_data_t *edata,
                                          void *user_data);
};
```

#### MotorSettings Class
```cpp
struct MotorSettings {
    uint32_t frequency = 10000;        // PWM frequency (Hz)
    float duty = 0.0;                  // PWM duty cycle (%)
    uint8_t polePairs = 2;             // Motor pole pairs
    uint32_t maxFrequency = 100000;    // Max frequency limit (Hz)
    uint8_t ledBrightness = 25;        // LED brightness (0-255)
    uint32_t rpmUpdateRate = 100;      // RPM update interval (ms)
    uint32_t maxSafeRPM = 500000;      // Overspeed threshold
};

class MotorSettingsManager {
public:
    bool begin();
    bool load();
    bool save();
    void reset();
    MotorSettings& get();

private:
    MotorSettings settings;
    Preferences preferences;
};
```

#### StatusLED Class
```cpp
class StatusLED {
public:
    void begin(int pin, uint8_t brightness = 25);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void setBlink(uint8_t r, uint8_t g, uint8_t b, uint32_t intervalMs);
    void setBrightness(uint8_t brightness);
    void update();  // Call in loop()
    void off();

private:
    int ledPin;
    uint8_t brightness;
    uint8_t currentR, currentG, currentB;
    bool blinkEnabled;
    uint32_t blinkInterval;
    uint32_t lastBlinkTime;
    bool blinkState;
};
```

### Extended Command System

#### New Commands

**Motor Control Commands:**
```
SET PWM_FREQ <Hz>           - Set PWM frequency (10-500000 Hz)
SET PWM_DUTY <%>            - Set PWM duty cycle (0-100%)
SET POLE_PAIRS <num>        - Set motor pole pairs (1-12)
SET MAX_FREQ <Hz>           - Set maximum frequency limit (1000-500000 Hz)
SET MAX_RPM <rpm>           - Set maximum safe RPM limit
SET LED_BRIGHTNESS <val>    - Set LED brightness (0-255)
RPM                         - Get current RPM and frequency
MOTOR STATUS                - Show detailed motor control status
MOTOR STOP                  - Emergency stop (set duty to 0%)
```

**Settings Commands:**
```
SAVE                        - Save all settings to NVS
LOAD                        - Load settings from NVS
RESET                       - Reset to factory defaults
```

#### Command Examples

**Via USB CDC:**
```
> SET PWM_FREQ 15000
✅ PWM frequency set to: 15000 Hz

> SET PWM_DUTY 75.5
✅ PWM duty cycle set to: 75.5%

> RPM
RPM Reading:
  Current RPM: 12450.5
  Input Frequency: 415.0 Hz
  Pole Pairs: 2
  PWM Frequency: 15000 Hz
  PWM Duty: 75.5%

> MOTOR STATUS
Motor Control Status:
  Initialization: ✅ OK
  PWM Unit: MCPWM_UNIT_1
  PWM Frequency: 15000 Hz
  PWM Duty: 75.5%

  Tachometer:
    Current RPM: 12450.5
    Input Frequency: 415.0 Hz
    Signal Status: ✅ Valid

  Configuration:
    Pole Pairs: 2
    Max Frequency: 100000 Hz
    Max Safe RPM: 500000
    Update Rate: 100 ms

  Safety:
    Overspeed: No
    Stall: No
    Emergency Stop: No

> MOTOR STOP
⚠️ Emergency stop activated - Duty set to 0%

> SAVE
✅ Settings saved to NVS

> LOAD
✅ Settings loaded from NVS
  PWM Frequency: 15000 Hz
  PWM Duty: 75.5%
  Pole Pairs: 2
```

**Via HID (0xA1 Protocol):**
```
Host → Device: [0xA1][0x0D][0x00]['S']['E']['T'][' ']['P']['W']['M']['_']['F']['R']['E']['Q'][' ']['1']['5']['0']['0']['0']['\n'][padding...]
Device → Host: [0xA1][0x1E][0x00]['✅'][' ']['P']['W']['M'][' ']['f']['r']['e']['q']['u']['e']['n']['c']['y'][' ']['s']['e']['t'][' ']['t']['o'][':'][' ']['1']['5']['0']['0']['0'][' ']['H']['z'][padding...]
```

**Via BLE:**
```
Client writes to RX: "SET PWM_FREQ 15000\n"
Client reads from TX (notify): "✅ PWM frequency set to: 15000 Hz\n"
```

### FreeRTOS Task Architecture

```
┌──────────────────────────────────────────────────────────┐
│                   Task Overview                           │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  ┌────────────────┐                                     │
│  │   HID Task     │  Priority: 2 (Highest)              │
│  │   Core: 1      │  Stack: 4096 bytes                  │
│  │                │  Function: Process HID data         │
│  │                │  Frequency: Event-driven (queue)    │
│  └────────────────┘                                     │
│                                                          │
│  ┌────────────────┐                                     │
│  │   CDC Task     │  Priority: 1 (Medium)               │
│  │   Core: 1      │  Stack: 4096 bytes                  │
│  │                │  Function: Process serial input     │
│  │                │  Frequency: 10ms polling            │
│  └────────────────┘                                     │
│                                                          │
│  ┌────────────────┐                                     │
│  │   BLE Task     │  Priority: 1 (Medium)               │
│  │   Core: 1      │  Stack: 4096 bytes                  │
│  │                │  Function: Process BLE commands     │
│  │                │  Frequency: Event-driven (queue)    │
│  └────────────────┘                                     │
│                                                          │
│  ┌────────────────┐                                     │
│  │  Motor Task    │  Priority: 1 (Medium) ← NEW         │
│  │   Core: 1      │  Stack: 4096 bytes                  │
│  │                │  Function: Update RPM, safety check │
│  │                │  Frequency: 100ms (configurable)    │
│  └────────────────┘                                     │
│                                                          │
│  ┌────────────────┐                                     │
│  │  IDLE Task     │  Priority: 0 (Lowest)               │
│  │ (Arduino loop) │  Stack: Default                     │
│  │                │  Function: LED update, system idle  │
│  │                │  Frequency: 100ms                   │
│  └────────────────┘                                     │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

#### Motor Task Implementation

```cpp
void motorTask(void* parameter) {
    TickType_t lastRPMUpdate = 0;
    TickType_t lastSafetyCheck = 0;

    while (true) {
        TickType_t now = xTaskGetTickCount();

        // Update RPM reading
        if (now - lastRPMUpdate >= pdMS_TO_TICKS(motorSettings.rpmUpdateRate)) {
            motorControl.updateRPM();
            lastRPMUpdate = now;
        }

        // Safety check every 500ms
        if (now - lastSafetyCheck >= pdMS_TO_TICKS(500)) {
            if (!motorControl.checkSafety()) {
                // Emergency stop triggered
                if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10))) {
                    USBSerial.println("\n⚠️ SAFETY ALERT: Emergency stop activated!");
                    xSemaphoreGive(serialMutex);
                }
                motorControl.emergencyStop();
                statusLED.setBlink(255, 0, 0, 500);  // Blink red
            }
            lastSafetyCheck = now;
        }

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### GPIO Pin Mapping

```
┌───────────────────────────────────────────────────────┐
│         ESP32-S3 Pin Assignments                      │
├───────────────────────────────────────────────────────┤
│  Function             │  GPIO  │  Peripheral          │
├───────────────────────┼────────┼──────────────────────┤
│  PWM Output           │   10   │  MCPWM1_A            │
│  Tachometer Input     │   11   │  MCPWM0_CAP0         │
│  Pulse Notification   │   12   │  Digital Out         │
│  RGB Status LED       │   48   │  RMT Channel 0       │
│  USB D-               │   19   │  USB OTG (Built-in)  │
│  USB D+               │   20   │  USB OTG (Built-in)  │
└───────────────────────────────────────────────────────┘
```

### Hardware Connection Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                  ESP32-S3 Hardware Connections              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  PWM Output (GPIO 10):                                      │
│    ESP32 GPIO10 ─── 1kΩ ───┬─── Motor Driver PWM Input     │
│                            └─── 100nF ─── GND              │
│                                                             │
│  Tachometer Input (GPIO 11):                                │
│    ESP32 GPIO11 ──┬── 10kΩ ──── 3.3V (Pull-up)            │
│                   └── 100nF ──── GND (Filter)              │
│    Tachometer Signal ────────────┘                         │
│                                                             │
│  Pulse Output (GPIO 12):                                    │
│    ESP32 GPIO12 ─── 1kΩ ─── LED (optional indicator)       │
│                                                             │
│  RGB LED (GPIO 48):                                         │
│    ESP32 GPIO48 ─── 470Ω ─── WS2812 DIN                   │
│    3.3V ────────────────────── WS2812 VCC                  │
│    GND ─────────────────────── WS2812 GND                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Status LED Color Codes

```
┌────────────────────────────────────────────────────┐
│  LED Color      │  Status                          │
├────────────────────────────────────────────────────┤
│  Green (Solid)  │  System ready, motor idle        │
│  Blue (Solid)   │  Motor running normally          │
│  Yellow (Blink) │  Initialization in progress      │
│  Red (Blink)    │  Error / Emergency stop          │
│  Purple (Solid) │  BLE connected                   │
│  Off            │  System not initialized          │
└────────────────────────────────────────────────────┘
```

---

## Implementation Phases

### Phase 1: Core Motor Control (Priority 1) ⭐⭐⭐

**Duration:** 1-2 hours
**Status:** In Progress

#### Tasks:

1. **Create MotorSettings Module** (15 min)
   - [x] Create `MotorSettings.h`
   - [ ] Create `MotorSettings.cpp`
   - [ ] Implement NVS load/save
   - [ ] Test settings persistence

2. **Create MotorControl Module** (45 min)
   - [ ] Create `MotorControl.h`
   - [ ] Create `MotorControl.cpp`
   - [ ] Implement MCPWM PWM output (GPIO 10)
   - [ ] Implement MCPWM Capture tachometer (GPIO 11)
   - [ ] Implement RPM calculation
   - [ ] Add safety checks
   - [ ] Test PWM output with oscilloscope

3. **Extend Command Parser** (20 min)
   - [ ] Add motor command handlers to `CommandParser.h`
   - [ ] Implement motor commands in `CommandParser.cpp`
   - [ ] Test commands via CDC console

4. **Update Main Application** (20 min)
   - [ ] Add motor control initialization to `setup()`
   - [ ] Create motor task in `setup()`
   - [ ] Update welcome message
   - [ ] Test complete system

#### Success Criteria:
- ✅ PWM output verified with oscilloscope
- ✅ Tachometer input reading correctly
- ✅ RPM calculation accurate
- ✅ Commands work from CDC, HID, BLE
- ✅ Settings save/load functional

---

### Phase 2: LED and Polish (Priority 2) ⭐⭐

**Duration:** 30 minutes
**Status:** Not Started

#### Tasks:

1. **Create StatusLED Module** (20 min)
   - [ ] Create `StatusLED.h`
   - [ ] Create `StatusLED.cpp`
   - [ ] Implement WS2812 control (GPIO 48)
   - [ ] Add blink functionality
   - [ ] Test LED colors

2. **Integrate LED with System** (10 min)
   - [ ] Update `main.cpp` loop for LED updates
   - [ ] Add LED status indicators
   - [ ] Test all color states

#### Success Criteria:
- ✅ LED shows correct colors for each state
- ✅ Blink mode works smoothly
- ✅ Brightness control functional

---

### Phase 3: Safety Features (Priority 3) ⭐

**Duration:** 1 hour
**Status:** Not Started

#### Tasks:

1. **Add RPM Filtering** (15 min)
   - [ ] Implement moving average filter
   - [ ] Add configurable filter size
   - [ ] Test filter stability

2. **Add Safety Monitoring** (20 min)
   - [ ] Implement overspeed detection
   - [ ] Implement stall detection
   - [ ] Add emergency stop mechanism
   - [ ] Test safety triggers

3. **Add Pulse Notification** (10 min)
   - [ ] Implement pulse output (GPIO 12)
   - [ ] Trigger on PWM changes
   - [ ] Test with logic analyzer

4. **Add Watchdog Timer** (15 min)
   - [ ] Configure ESP32 watchdog
   - [ ] Add watchdog feed in tasks
   - [ ] Test watchdog recovery

#### Success Criteria:
- ✅ RPM readings stable and filtered
- ✅ Overspeed protection works
- ✅ Stall detection functional
- ✅ Watchdog prevents system hang

---

### Phase 4: Advanced Features (Priority 4 - Optional) ⏸️

**Duration:** 2-3 hours
**Status:** Deferred

#### Tasks:

1. **PWM Ramping**
   - [ ] Implement smooth frequency ramping
   - [ ] Implement smooth duty cycle ramping
   - [ ] Add configurable ramp time

2. **Acceleration Calculation**
   - [ ] Calculate RPM/s
   - [ ] Expose via STATUS command
   - [ ] Add to API response

3. **Advanced MCPWM Features**
   - [ ] Add deadtime control
   - [ ] Add complementary PWM output
   - [ ] Add fault protection input
   - [ ] Add MCPWM synchronization

4. **Signal Quality Monitoring**
   - [ ] Track valid/rejected pulses
   - [ ] Calculate jitter
   - [ ] Expose metrics via API

---

## Testing Plan

### Unit Tests

#### MotorControl Tests
```cpp
// Test PWM frequency setting
void test_pwm_frequency() {
    motorControl.setPWMFrequency(10000);
    assert(motorControl.getPWMFrequency() == 10000);
}

// Test PWM duty setting
void test_pwm_duty() {
    motorControl.setPWMDuty(50.0);
    assert(motorControl.getPWMDuty() == 50.0);
}

// Test RPM calculation
void test_rpm_calculation() {
    // Inject test frequency
    // Verify RPM calculation
}
```

#### MotorSettings Tests
```cpp
// Test NVS save/load
void test_settings_persistence() {
    settings.get().frequency = 15000;
    settings.save();

    settings.reset();  // Clear
    settings.load();

    assert(settings.get().frequency == 15000);
}
```

### Integration Tests

#### Command Interface Tests
```
Test: CDC Command Processing
1. Send: "SET PWM_FREQ 15000\n"
2. Verify: Response "✅ PWM frequency set to: 15000 Hz"
3. Send: "MOTOR STATUS\n"
4. Verify: Frequency shows 15000 Hz

Test: HID Command Processing
1. Send: [0xA1][0x0D][0x00]['S']['E']['T'][' ']['P']['W']['M']['_']['F']['R']['E']['Q'][' ']['1']['5']['0']['0']['0']['\n']
2. Verify: Response [0xA1][length][0x00][response text]

Test: BLE Command Processing
1. Write to RX: "SET PWM_FREQ 15000\n"
2. Read from TX: "✅ PWM frequency set to: 15000 Hz\n"
```

### Hardware Tests

#### PWM Output Verification
```
Equipment: Oscilloscope
Pin: GPIO 10

Test 1: Frequency Accuracy
1. Set frequency: 10 kHz
2. Measure: Should be 10.000 kHz ± 10 Hz
3. Repeat for: 100 Hz, 1 kHz, 10 kHz, 100 kHz

Test 2: Duty Cycle Accuracy
1. Set duty: 50%
2. Measure: Should be 50.0% ± 0.1%
3. Repeat for: 0%, 25%, 50%, 75%, 100%

Test 3: Glitch-Free Switching
1. Change frequency from 1 kHz to 10 kHz
2. Verify: No glitches during transition
```

#### Tachometer Input Verification
```
Equipment: Function Generator, Frequency Counter
Pin: GPIO 11

Test 1: Frequency Measurement
1. Input: 100.00 Hz signal
2. Verify: System reads 100.0 Hz ± 0.1 Hz
3. Repeat for: 1 Hz, 10 Hz, 100 Hz, 1 kHz, 10 kHz

Test 2: RPM Calculation
1. Set pole pairs: 2
2. Input: 100.00 Hz signal
3. Expected RPM: (100 × 60) / 2 = 3000 RPM
4. Verify: System reads 3000 ± 3 RPM
```

#### LED Status Verification
```
Equipment: Visual inspection
Pin: GPIO 48

Test: All Colors
1. Green (Ready): OK
2. Blue (Running): OK
3. Yellow (Init): OK
4. Red (Error): OK
5. Purple (BLE): OK
6. Brightness control: OK
7. Blink mode: OK
```

### System Tests

#### Multi-Interface Concurrent Access
```
Test: Simultaneous Commands
1. CDC sends: "SET PWM_FREQ 15000"
2. HID sends: "SET PWM_DUTY 50"
3. BLE sends: "RPM"
4. Verify: All commands processed correctly
5. Verify: No race conditions
6. Verify: Correct responses to each interface
```

#### Long-Term Stability
```
Test: 24-Hour Run
1. Set PWM: 10 kHz, 50% duty
2. Monitor RPM continuously
3. Send random commands every 10 seconds
4. Verify: No crashes, no memory leaks
5. Check: Free heap remains stable
```

---

## Future Enhancements

### Phase 5: WiFi Integration (Optional)

**Tasks:**
- Add WiFi connection management
- Implement web server
- Create web dashboard UI
- Add REST API endpoints
- Add WebSocket for real-time updates
- Implement OTA firmware updates

**Estimated Time:** 4-6 hours

### Phase 6: Advanced Motor Control

**Tasks:**
- Add closed-loop speed control (PID)
- Add torque control mode
- Add position control mode
- Implement motor profiling
- Add thermal monitoring
- Add current monitoring

**Estimated Time:** 8-10 hours

### Phase 7: Data Logging

**Tasks:**
- Add SD card support
- Implement data logging to SD
- Add CSV export
- Create data visualization
- Add remote data retrieval

**Estimated Time:** 3-4 hours

---

## Risk Assessment

### Technical Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| MCPWM conflicts with existing code | High | Low | Use separate MCPWM units |
| Memory exhaustion | High | Low | Monitor heap, optimize buffers |
| Task priority conflicts | Medium | Low | Proper priority assignment |
| PWM frequency limitations | Medium | Low | Validate ranges in code |
| Tachometer noise issues | Medium | Medium | Add hardware filtering |

### Development Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Breaking existing functionality | High | Low | Incremental changes, testing |
| Integration complexity | Medium | Medium | Modular design, clear interfaces |
| Testing difficulties | Medium | Medium | Hardware test fixtures |
| Documentation gaps | Low | Medium | Comprehensive inline comments |

---

## Success Metrics

### Functional Requirements
- ✅ PWM frequency range: 10 Hz - 500 kHz
- ✅ PWM duty cycle range: 0% - 100%
- ✅ RPM measurement accuracy: ±0.5%
- ✅ Command response time: < 50 ms
- ✅ All three interfaces operational
- ✅ Settings persistence working

### Performance Requirements
- ✅ RPM update rate: 100 ms (configurable)
- ✅ PWM switching time: < 1 ms (glitch-free)
- ✅ Free heap after initialization: > 150 KB
- ✅ Task stack usage: < 3 KB per task
- ✅ System uptime: > 24 hours continuous

### Quality Requirements
- ✅ No compiler warnings
- ✅ No memory leaks
- ✅ Clean error handling
- ✅ Comprehensive documentation
- ✅ All tests passing

---

## References

- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [MCPWM API Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/mcpwm.html)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [Existing Project Documentation](CLAUDE.md)

---

## Appendix A: Command Reference

### Motor Control Commands

```
SET PWM_FREQ <Hz>           Set PWM frequency (10-500000 Hz)
SET PWM_DUTY <%>            Set PWM duty cycle (0-100%)
SET POLE_PAIRS <num>        Set motor pole pairs (1-12)
SET MAX_FREQ <Hz>           Set maximum frequency limit
SET MAX_RPM <rpm>           Set maximum RPM limit
SET LED_BRIGHTNESS <val>    Set LED brightness (0-255)
RPM                         Get current RPM reading
MOTOR STATUS                Show detailed motor status
MOTOR STOP                  Emergency stop
SAVE                        Save settings to NVS
LOAD                        Load settings from NVS
RESET                       Reset to factory defaults
```

### Response Format

**Success:**
```
✅ <message>
```

**Error:**
```
❌ <error message>
```

**Warning:**
```
⚠️ <warning message>
```

**Info:**
```
ℹ️ <info message>
```

---

## Appendix B: Configuration Defaults

```cpp
// Default Settings
const uint32_t DEFAULT_FREQUENCY = 10000;      // 10 kHz
const float DEFAULT_DUTY = 0.0;                // 0% (motor off)
const uint8_t DEFAULT_POLE_PAIRS = 2;          // 2 pole pairs
const uint32_t DEFAULT_MAX_FREQUENCY = 100000; // 100 kHz
const uint32_t DEFAULT_MAX_RPM = 500000;       // 500k RPM
const uint8_t DEFAULT_LED_BRIGHTNESS = 25;     // 10% brightness
const uint32_t DEFAULT_RPM_UPDATE_RATE = 100;  // 100 ms

// Hardware Pin Definitions
const int PWM_OUTPUT_PIN = 10;                 // GPIO 10
const int TACHOMETER_INPUT_PIN = 11;           // GPIO 11
const int PULSE_OUTPUT_PIN = 12;               // GPIO 12
const int STATUS_LED_PIN = 48;                 // GPIO 48

// MCPWM Configuration
const mcpwm_unit_t PWM_MCPWM_UNIT = MCPWM_UNIT_1;
const mcpwm_timer_t PWM_MCPWM_TIMER = MCPWM_TIMER_0;
const mcpwm_io_signals_t PWM_MCPWM_GEN = MCPWM0A;

const mcpwm_unit_t CAP_MCPWM_UNIT = MCPWM_UNIT_0;
const mcpwm_capture_signal_t CAP_SIGNAL = MCPWM_CAP_0;

// Safety Limits
const uint32_t MIN_PWM_FREQUENCY = 10;         // 10 Hz
const uint32_t MAX_PWM_FREQUENCY = 500000;     // 500 kHz
const float MIN_DUTY_CYCLE = 0.0;              // 0%
const float MAX_DUTY_CYCLE = 100.0;            // 100%
const uint8_t MIN_POLE_PAIRS = 1;
const uint8_t MAX_POLE_PAIRS = 12;
```

---

**End of Integration Plan**
