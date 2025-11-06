# Priority 3 Features Testing Guide

This document provides testing procedures for the Priority 3 advanced features: RPM filtering, PWM ramping, and watchdog timer.

## Features Overview

### 1. RPM Filtering (Moving Average)
- **Purpose**: Smooth out noisy RPM readings from the tachometer
- **Implementation**: Circular buffer with configurable window size (1-20 samples)
- **Default**: 5-sample moving average
- **Benefits**: Reduces jitter in RPM displays, more stable readings

### 2. PWM Ramping
- **Purpose**: Gradual transitions for PWM frequency and duty cycle
- **Implementation**: Linear interpolation over configurable time period
- **Benefits**:
  - Smoother motor acceleration/deceleration
  - Reduced mechanical stress on motor
  - Better control for sensitive applications
  - LED indicator shows yellow during ramping

### 3. Watchdog Timer
- **Purpose**: Automatic safety timeout if motor task hangs
- **Implementation**: Software watchdog with 5-second timeout
- **Benefits**:
  - Detects system hangs or deadlocks
  - Triggers emergency stop on timeout
  - Auto-recovery mechanism

## New Commands

### RPM Filtering Commands

```
SET RPM_FILTER_SIZE <n>
```
- Set the moving average window size
- Valid range: 1-20 samples
- Example: `SET RPM_FILTER_SIZE 10` (use 10-sample average)

```
FILTER STATUS
```
- Display current filter status
- Shows:
  - Filter window size
  - Raw (unfiltered) RPM
  - Filtered RPM
  - Filtering difference
  - Ramping status (if active)

### PWM Ramping Commands

```
RAMP PWM_FREQ <Hz> <ms>
```
- Ramp PWM frequency to target value over specified time
- Example: `RAMP PWM_FREQ 25000 2000` (ramp to 25kHz over 2 seconds)

```
RAMP PWM_DUTY <%> <ms>
```
- Ramp PWM duty cycle to target value over specified time
- Example: `RAMP PWM_DUTY 75.0 1500` (ramp to 75% over 1.5 seconds)

### Viewing Status

```
MOTOR STATUS
```
- Now includes advanced features section showing:
  - RPM filter size
  - Raw RPM vs Filtered RPM
  - Ramping status
  - Watchdog status

## Test Procedures

### Test 1: RPM Filter Configuration

**Objective**: Verify RPM filtering reduces noise

**Procedure**:
1. Connect motor with tachometer feedback
2. Set motor to moderate speed:
   ```
   SET PWM_DUTY 50
   ```

3. Test with no filtering (size=1):
   ```
   SET RPM_FILTER_SIZE 1
   FILTER STATUS
   ```
   - Note: Raw RPM = Filtered RPM

4. Enable filtering:
   ```
   SET RPM_FILTER_SIZE 5
   ```
   - Wait 5 RPM update cycles (500ms with default update rate)

5. Check filter effect:
   ```
   FILTER STATUS
   ```
   - Filtered RPM should be smoother than raw RPM

6. Test larger filter:
   ```
   SET RPM_FILTER_SIZE 10
   FILTER STATUS
   ```
   - More aggressive smoothing (larger lag)

**Expected Results**:
- ✅ Filter size changes accepted (1-20 range)
- ✅ Filtered RPM is smoother than raw RPM
- ✅ Larger filter = more smoothing, more lag
- ✅ Invalid sizes rejected with error message

### Test 2: PWM Frequency Ramping

**Objective**: Verify smooth frequency transitions

**Procedure**:
1. Set initial frequency:
   ```
   SET PWM_FREQ 15000
   SET PWM_DUTY 30
   ```

2. Test fast ramp:
   ```
   RAMP PWM_FREQ 25000 1000
   ```
   - LED should turn yellow during ramp
   - Watch serial output for progress

3. Verify final frequency:
   ```
   MOTOR STATUS
   ```
   - Should show 25000 Hz

4. Test slow ramp:
   ```
   RAMP PWM_FREQ 10000 5000
   ```
   - Ramp takes 5 seconds
   - Smooth transition observable on oscilloscope

5. Test immediate ramp (time=0):
   ```
   RAMP PWM_FREQ 20000 0
   ```
   - Should behave like `SET PWM_FREQ 20000`

**Expected Results**:
- ✅ Frequency changes gradually over specified time
- ✅ LED shows yellow during ramping
- ✅ Final frequency matches target
- ✅ Zero ramp time = immediate change
- ✅ Invalid ranges rejected

### Test 3: PWM Duty Ramping

**Objective**: Verify smooth duty cycle transitions

**Procedure**:
1. Set initial state:
   ```
   SET PWM_DUTY 10
   ```

2. Test acceleration ramp:
   ```
   RAMP PWM_DUTY 80 3000
   ```
   - Motor should accelerate smoothly over 3 seconds
   - No sudden jumps

3. Check final duty:
   ```
   RPM
   MOTOR STATUS
   ```

4. Test deceleration ramp:
   ```
   RAMP PWM_DUTY 20 2000
   ```
   - Motor should decelerate smoothly

5. Test full stop:
   ```
   RAMP PWM_DUTY 0 1000
   ```
   - Gradual stop over 1 second

**Expected Results**:
- ✅ Duty cycle changes smoothly
- ✅ Motor acceleration/deceleration smooth
- ✅ No mechanical shock or vibration
- ✅ Final duty matches target
- ✅ Can interrupt ramp with new command

### Test 4: Watchdog Timer

**Objective**: Verify watchdog timeout detection

**Procedure**:
1. Enable watchdog by starting motor:
   ```
   SET PWM_DUTY 50
   ```
   - Watchdog automatically enabled when motor task feeds it

2. Monitor normal operation:
   ```
   MOTOR STATUS
   ```
   - Watchdog status should show "✅ 正常"

3. Simulate watchdog timeout (requires code modification):
   - Temporarily add delay in motor task or comment out feedWatchdog()
   - Rebuild and upload
   - Wait 5+ seconds

4. Check watchdog alert:
   - Serial console should show "⚠️ WATCHDOG ALERT: Timeout detected"
   - Emergency stop activated
   - LED blinks red

**Expected Results**:
- ✅ Watchdog status shown in MOTOR STATUS
- ✅ Timeout detected after 5 seconds of no feeding
- ✅ Emergency stop triggered automatically
- ✅ LED indicates error state (blinking red)

### Test 5: Combined Features

**Objective**: Verify all features work together

**Procedure**:
1. Configure system:
   ```
   SET RPM_FILTER_SIZE 5
   SET PWM_FREQ 15000
   SET PWM_DUTY 0
   ```

2. Perform ramped startup:
   ```
   RAMP PWM_DUTY 70 3000
   ```
   - Smooth acceleration
   - Filtered RPM readings
   - Watchdog active

3. Monitor during ramp:
   ```
   FILTER STATUS
   ```
   - Shows ramping in progress
   - Shows filtered RPM

4. Verify final state:
   ```
   MOTOR STATUS
   ```
   - All features operational
   - Watchdog OK
   - Filter active
   - No ramping

5. Test emergency stop:
   ```
   MOTOR STOP
   ```
   - Immediate stop (bypasses ramping)
   - Watchdog still active

**Expected Results**:
- ✅ All features work simultaneously
- ✅ No interference between features
- ✅ Emergency stop always immediate
- ✅ System stable and responsive

## Visual Indicators

### LED States for Priority 3 Features

| LED Color | Meaning |
|-----------|---------|
| Green | Motor idle, all systems normal |
| Blue | Motor running, steady state |
| **Yellow** | **PWM ramping in progress** (Priority 3) |
| Purple | BLE client connected |
| Blinking Red | Error (safety alert or watchdog timeout) |

## Troubleshooting

### Issue: Filter Not Smoothing

**Symptoms**: Filtered RPM = Raw RPM even with large filter size

**Solutions**:
1. Verify filter size > 1
2. Wait for filter buffer to fill (filter_size × update_rate)
3. Check that motor is actually running and generating tachometer pulses

### Issue: Ramp Not Working

**Symptoms**: PWM changes immediately instead of gradually

**Solutions**:
1. Check ramp time is > 0
2. Verify MOTOR STATUS shows "進行中" during ramp
3. Check serial output for ramp start message
4. Ensure motor task is running (watchdog OK)

### Issue: Watchdog False Triggers

**Symptoms**: Watchdog timeout despite system working

**Solutions**:
1. Check motor task priority and CPU load
2. Verify motor task delay is < 100ms
3. Check for blocking operations in motor task
4. Increase WATCHDOG_TIMEOUT_MS if needed (MotorControl.h:258)

## Performance Notes

### Filter Performance
- **Memory**: 80 bytes for 20-sample buffer (4 bytes × 20)
- **CPU**: Minimal (~10 µs for averaging)
- **Latency**: filter_size × update_rate (e.g., 5 samples × 100ms = 500ms settling time)

### Ramp Performance
- **Update Rate**: 10ms (motor task loop)
- **Resolution**: Time-based linear interpolation
- **CPU**: Minimal (simple arithmetic per 10ms)
- **Precision**: Float for duty, uint32_t for frequency

### Watchdog Performance
- **Timeout**: 5000ms (configurable)
- **Feed Interval**: 100ms (motor task)
- **Overhead**: Trivial (timestamp comparison)
- **Safety Margin**: 50× (5000ms / 100ms)

## Advanced Testing

### Oscilloscope Verification

1. **Frequency Ramping**:
   - Connect scope to GPIO 10 (PWM output)
   - Trigger: Single
   - Use frequency counter
   - Verify smooth frequency transition

2. **Duty Ramping**:
   - Measure duty cycle over time
   - Should show linear increase/decrease
   - No glitches or steps

### Logic Analyzer Verification

1. **Pulse Output** (GPIO 12):
   - Captures 10µs pulse on PWM changes
   - Verify pulse on ramp completion
   - Should NOT pulse during ramp (only at end)

2. **Timing Verification**:
   - Measure actual ramp duration
   - Should match commanded time ±10ms

## Integration with Existing Features

### Priority 1 Compatibility
- ✅ PWM control (GPIO 10) unchanged
- ✅ Tachometer input (GPIO 11) enhanced with filtering
- ✅ Settings storage compatible

### Priority 2 Compatibility
- ✅ RGB LED (GPIO 48) adds yellow state for ramping
- ✅ All other LED states preserved

## Next Steps (Priority 4 - Optional)

Potential future enhancements:
- Acceleration calculation (RPM/s)
- S-curve ramping (non-linear)
- Adaptive filtering based on RPM
- Hardware watchdog integration (ESP32-S3 TWDT)
- Fault input monitoring (GPIO-based)

## Summary

Priority 3 features provide:
1. ✅ **Cleaner RPM readings** via moving average filter
2. ✅ **Smoother motor control** via PWM ramping
3. ✅ **Enhanced safety** via watchdog timer
4. ✅ **Full command interface** for all features
5. ✅ **Visual feedback** via LED (yellow for ramping)

All features are production-ready and have been integrated into the existing command system.
