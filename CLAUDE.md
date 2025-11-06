# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-S3 USB composite device implementation combining CDC (Communications Device Class) and custom HID (Human Interface Device) functionality. The device presents two USB interfaces:
- **CDC**: Serial console for command input/output
- **Custom HID**: 64-byte bidirectional data reports (IN/OUT)

**Hardware Platform:**
- Board: ESP32-S3-DevKitC-1 N16R8
- Chip: ESP32-S3
- Flash: 16 MB Quad SPI Flash
- PSRAM: 8 MB Octal PSRAM
- USB: USB OTG support

## Changing Board Variants

This project is configured for **ESP32-S3-DevKitC-1 N16R8** by default, but can be adapted to other ESP32-S3-DevKitC-1 variants. Here's how to switch boards:

### Supported Board Variants

| Board Variant | Flash Size | PSRAM Size | PlatformIO Board ID |
|--------------|-----------|-----------|-------------------|
| **N16R8** (Default) | 16 MB | 8 MB | `esp32-s3-devkitc-1-n16r8` |
| **N8R2** | 8 MB | 2 MB | `esp32-s3-devkitc-1-n8r2` |
| **N8R8** | 8 MB | 8 MB | `esp32-s3-devkitc-1-n8r8` |
| **Standard/N8** | 8 MB | No PSRAM | `esp32-s3-devkitc-1-n8` |
| **N4R2** | 4 MB | 2 MB | `esp32-s3-devkitc-1` (generic) |

### Step-by-Step Guide

**Important Note About Board Definitions:**

The official PlatformIO `platform-espressif32` only includes the base `esp32-s3-devkitc-1` board (N8 variant). For other variants (N16R8, N8R2, N8R8), you have **two options**:

- **Option A**: Use base board with overrides (recommended - simpler, no extra files)
- **Option B**: Use variant-specific board IDs (if available in your platform version)

**1. Update platformio.ini**

Choose the appropriate configuration method:

**Option A: For N16R8 using base board + overrides (RECOMMENDED):**
```ini
[env:esp32-s3-devkitc-1-n16r8]
platform = espressif32
board = esp32-s3-devkitc-1  ; Use base board
framework = arduino
; Override flash/PSRAM settings for N16R8
board_build.flash_size = 16MB
board_build.psram_type = opi
board_build.memory_type = qio_opi
board_upload.flash_size = 16MB
board_upload.maximum_size = 16777216
board_build.partitions = default_16MB.csv
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCONFIG_TINYUSB_HID_BUFSIZE=128
    -DCFG_TUD_HID_EP_BUFSIZE=128
    -DBOARD_HAS_PSRAM
monitor_speed = 115200
```

**Option B: For N16R8 using specific board ID (if available):**
```ini
[env:esp32-s3-devkitc-1-n16r8]
platform = espressif32
board = esp32-s3-devkitc-1-n16r8  ; May not exist in older platforms
framework = arduino
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCONFIG_TINYUSB_HID_BUFSIZE=128
    -DCFG_TUD_HID_EP_BUFSIZE=128
monitor_speed = 115200
```

**For N8R2 (8MB Flash, 2MB PSRAM) - Option A:**
```ini
[env:esp32-s3-devkitc-1-n8r2]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_size = 8MB
board_build.psram_type = qspi
board_build.memory_type = qio_qspi
board_upload.flash_size = 8MB
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCONFIG_TINYUSB_HID_BUFSIZE=128
    -DCFG_TUD_HID_EP_BUFSIZE=128
    -DBOARD_HAS_PSRAM
monitor_speed = 115200
```

**For Standard ESP32-S3-DevKitC-1 (8MB Flash, no PSRAM):**
```ini
[env:esp32-s3-devkitc-1-n8]
platform = espressif32
board = esp32-s3-devkitc-1  ; This is the default
framework = arduino
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCONFIG_TINYUSB_HID_BUFSIZE=128
    -DCFG_TUD_HID_EP_BUFSIZE=128
monitor_speed = 115200
```

**2. Clean Build (Critical!)**

After changing the board configuration, you **must** perform a clean build:

```bash
pio run -t clean && pio run
```

This ensures:
- Correct partition table is loaded for the new flash size
- PSRAM configuration is properly applied
- All build artifacts are regenerated for the new target

**3. Upload Firmware**

Enter bootloader mode and upload as usual:
```bash
# Enter bootloader mode: Hold BOOT, press RESET, release BOOT
pio run -t upload
```

**4. Verify Configuration**

After uploading, use the `INFO` command to verify the board configuration:

```
*IDN?  → Returns: HID_ESP32_S3
INFO   → Should show correct Flash and PSRAM sizes
```

### Important Considerations

**Flash Size Impact:**
- **16MB → 8MB**: No issues, the project uses minimal flash (~300KB)
- **16MB → 4MB**: Still sufficient, but less room for future expansion
- The firmware size is ~307KB, well within limits for all variants

**PSRAM Impact:**
- **With PSRAM (8MB/2MB)**: FreeRTOS tasks can use larger stacks if needed
- **Without PSRAM (N8 variant)**: Current configuration works fine (4KB per task)
- The current implementation uses ~700 bytes for queue + minimal mutex overhead
- Total RAM usage: ~31KB (well within ESP32-S3's 512KB internal SRAM)

**No Code Changes Required:**
- The USB composite device implementation is independent of flash/PSRAM size
- Build flags remain the same across all variants
- FreeRTOS configuration (4KB stack per task) works on all variants

### Partition Table Notes

PlatformIO automatically selects the appropriate partition table based on the board variant:
- **N16R8**: Uses 16MB flash partition scheme
- **N8R2/N8R8/N8**: Uses 8MB flash partition scheme
- **Generic (4MB)**: Uses default 4MB partition scheme

You do **not** need to manually specify partition tables unless you have custom requirements.

### Verifying Your Board Variant

If you're unsure which variant you have, check the board label:

```
ESP32-S3-DevKitC-1-N16R8  → 16MB Flash, 8MB PSRAM
ESP32-S3-DevKitC-1-N8R2   → 8MB Flash, 2MB PSRAM
ESP32-S3-DevKitC-1-N8R8   → 8MB Flash, 8MB PSRAM
ESP32-S3-DevKitC-1-N8     → 8MB Flash, No PSRAM
ESP32-S3-DevKitC-1        → Usually 4MB Flash, 2MB PSRAM (check marking)
```

The variant code breakdown:
- **N16** = 16MB Flash, **N8** = 8MB Flash, **N4** = 4MB Flash
- **R8** = 8MB PSRAM, **R2** = 2MB PSRAM, no R = No PSRAM

### Custom Board JSON Files (Advanced)

If you prefer to create custom board definitions instead of using `board_*` overrides, you can create JSON files in your project:

**1. Create a `boards/` directory in your project root:**

```bash
mkdir boards
```

**2. Create `boards/esp32-s3-devkitc-1-n16r8.json`:**

```json
{
  "build": {
    "arduino": {
      "ldscript": "esp32s3_out.ld",
      "memory_type": "qio_opi"
    },
    "core": "esp32",
    "extra_flags": [
      "-DARDUINO_ESP32S3_DEV",
      "-DARDUINO_RUNNING_CORE=1",
      "-DARDUINO_EVENT_RUNNING_CORE=1",
      "-DBOARD_HAS_PSRAM"
    ],
    "f_cpu": "240000000L",
    "f_flash": "80000000L",
    "flash_mode": "qio",
    "hwids": [["0x303A", "0x1001"]],
    "mcu": "esp32s3",
    "variant": "esp32s3"
  },
  "connectivity": ["wifi", "bluetooth"],
  "debug": {
    "openocd_target": "esp32s3.cfg"
  },
  "frameworks": ["arduino", "espidf"],
  "name": "ESP32-S3-DevKitC-1-N16R8 (16 MB QD, 8 MB Octal PSRAM)",
  "upload": {
    "flash_size": "16MB",
    "maximum_ram_size": 327680,
    "maximum_size": 16777216,
    "require_upload_port": true,
    "speed": 921600
  },
  "url": "https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html",
  "vendor": "Espressif"
}
```

**3. Use simplified platformio.ini:**

```ini
[env:esp32-s3-devkitc-1-n16r8]
platform = espressif32
board = esp32-s3-devkitc-1-n16r8  ; PlatformIO will find it in boards/ folder
framework = arduino
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCONFIG_TINYUSB_HID_BUFSIZE=128
    -DCFG_TUD_HID_EP_BUFSIZE=128
monitor_speed = 115200
```

**Advantages of Custom JSON:**
- ✅ Cleaner `platformio.ini` configuration
- ✅ Better IDE autocomplete and board recognition
- ✅ Reusable across projects

**Disadvantages:**
- ⚠️ Requires maintaining extra files
- ⚠️ May conflict with future platform updates

**Recommendation:** Use **Option A** (board overrides) unless you need the JSON file for multiple projects or prefer cleaner configuration.

## Build Commands

```bash
# Build firmware
pio run

# Clean build (recommended after changing platformio.ini)
pio run -t clean && pio run

# Upload firmware (requires bootloader mode)
# 1. Hold BOOT button, press RESET, release BOOT
# 2. Then run:
pio run -t upload

# Open serial monitor (must enable DTR in terminal)
pio device monitor
```

**Important:** After uploading new firmware, physically disconnect and reconnect the USB cable to ensure proper USB enumeration.

## Critical Configuration

### USB Mode Requirements
The `platformio.ini` must include these build flags for USB functionality:
```ini
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCONFIG_TINYUSB_HID_BUFSIZE=128
    -DCFG_TUD_HID_EP_BUFSIZE=128
```

**Important:** The TinyUSB HID buffer size flags are set to 128 to provide sufficient buffer space, though the actual data payload is 64 bytes.

### DTR Signal Dependency
The CDC serial interface (`USBSerial`) requires DTR (Data Terminal Ready) to be enabled in the serial terminal. Without DTR:
- The device will wait 5 seconds at boot (timeout in `setup()` at line 59)
- Serial communication still works after timeout
- Welcome message may be missed if terminal opens late

This is intentional behavior to ensure reliable data reception and should be maintained.

## Code Structure

### Key Files
- **`src/main.cpp`**: Main application loop, USB initialization, HID/CDC data routing
- **`src/CustomHID.h/cpp`**: Custom 64-byte HID device class without Report ID
- **`src/CommandParser.h/cpp`**: Unified command parser with dual-channel response system
- **`platformio.ini`**: Critical USB build flags configuration

### Architecture Overview

**Dual-Interface Design:**
- Two independent USB interfaces: CDC (serial console) + HID (64-byte data)
- Unified command system: Both CDC and HID can send same text commands
- Response routing:
  - CDC commands → CDC response only
  - HID commands → CDC + HID response (dual channel)

**FreeRTOS Multi-Tasking Architecture:**
This implementation uses FreeRTOS to separate concerns and ensure thread-safe operation:

**Tasks:**
1. **HID Task** (`hidTask`, Priority 2, Core 1)
   - Receives HID data from queue (populated by ISR)
   - Detects command packets (0xA1 protocol or plain text) vs raw data
   - Processes HID commands via CommandParser with MultiChannelResponse
   - Displays protocol type and debug information for commands and raw data

2. **CDC Task** (`cdcTask`, Priority 1, Core 1)
   - Polls CDC serial input (no character echo)
   - Accumulates characters into command buffer
   - Processes CDC commands via CommandParser with CDCResponse

3. **IDLE Task** (Arduino `loop()`)
   - Minimal work, yields to FreeRTOS scheduler

**Synchronization Mechanisms:**
- `hidDataQueue`: Queue for passing HID data from ISR to HID Task (10 packets deep)
- `serialMutex`: Protects `USBSerial` access from multiple tasks
- `bufferMutex`: Protects `hid_out_buffer` shared memory
- `hidSendMutex`: Protects `HID.send()` calls to prevent concurrent access

**Data Flow:**
```
ISR Context:
  onHIDData() → xQueueSendFromISR(hidDataQueue) → portYIELD_FROM_ISR()

HID Task Context:
  xQueueReceive(hidDataQueue) → Process Data → CommandParser → HIDResponse (with hidSendMutex)

CDC Task Context:
  USBSerial.read() → CommandParser → CDCResponse (with serialMutex)
```

**Thread Safety:**
- All access to `USBSerial` is protected by `serialMutex`
- All access to `hid_out_buffer` is protected by `bufferMutex`
- All access to `HID.send()` is protected by `hidSendMutex`
- ISR uses non-blocking queue send (`xQueueSendFromISR`)
- No complex operations in ISR context (only data copy to queue)

### USB Initialization Sequence
The initialization order in `setup()` is critical:
1. `USBSerial.begin()` - Initialize CDC interface
2. `HID.begin()` - Initialize custom HID interface
3. `HID.onData(onHIDData)` - Register HID data callback (ISR)
4. `USB.begin()` - Start USB stack (enumerates both CDC and HID)
5. Create response objects:
   - `CDCResponse` - For CDC-only responses
   - `HIDResponse` - For HID responses
   - `MultiChannelResponse` - For dual CDC+HID responses
6. Wait for `USBSerial` connection (max 5s timeout)
7. **Create FreeRTOS resources:**
   - `hidDataQueue` - Queue for HID data packets (10 deep)
   - `serialMutex` - Mutex for USBSerial protection
   - `bufferMutex` - Mutex for hid_out_buffer protection
   - `hidSendMutex` - Mutex for HID.send() protection
8. **Create FreeRTOS tasks:**
   - `hidTask` (Priority 2, 4KB stack, Core 1)
   - `cdcTask` (Priority 1, 4KB stack, Core 1)

**Note:**
- The HID interface is available immediately after `USB.begin()`, independent of the CDC serial connection
- FreeRTOS tasks must be created after USB initialization to ensure proper operation
- Both tasks run on Core 1 to avoid conflicts with Arduino WiFi/BT on Core 0

### HID Communication

**HID Instance Configuration:**
```cpp
CustomHID64 HID;  // 64-byte custom HID without Report ID
```

**Critical Design Decision: No Report ID**

This implementation uses a **Report ID-free** HID descriptor to achieve true 64-byte data transfers. This design choice was made to overcome a fundamental USB limitation:

- USB Full Speed Interrupt endpoints have a maximum packet size of **64 bytes**
- With Report ID: Total size = 65 bytes (1 Report ID + 64 data) → Requires 2 USB packets
- Without Report ID: Total size = 64 bytes (pure data) → Single USB packet transfer
- TinyUSB's HID implementation only processes the first packet when using Report ID, resulting in only 63 bytes received

**HID Report Descriptor (`CustomHID.cpp:8-28`):**
- Vendor-defined Usage Page (0xFF00)
- No Report ID field
- 64-byte Input Report (Device → Host)
- 64-byte Output Report (Host → Device)
- Report Descriptor Length: 32 bytes

**OUT Report (Host → Device):**
- Received via `_onOutput()` callback in `CustomHID64` class
- Full 64 bytes delivered to `rx_buffer[64]`
- Data callback registered via `HID.onData(callback)`
- Main application receives data in `onHIDData()` (see `main.cpp`)

**IN Report (Device → Host):**
- Send via `HID.send(buffer, 64)`
- Takes pure data buffer (no Report ID needed)
- Returns `true` on success
- Example: See `processCommand()` "send" case in `main.cpp`

**Dual Protocol Support:**

The device supports **two HID command protocols** for maximum flexibility:

1. **0xA1 Protocol (Structured)**
   - Format: `[0xA1][length][0x00][command_text...]` (3-byte header + command)
   - Header size: 3 bytes
   - Max command length: 61 bytes
   - Example: `[0xA1][0x06][0x00]['*']['I']['D']['N']['?']['\n'][padding...]`
   - Recommended for structured applications requiring explicit framing

2. **Plain Text Protocol (Simple)**
   - Format: `[command_text...]` (direct text, no header)
   - Commands must end with newline (`\n` or `\r`)
   - Max command length: 64 bytes (including newline)
   - Example: `['*']['I']['D']['N']['?']['\n'][padding...]`
   - Recommended for simple testing and Bus Hound analysis

**Protocol Detection:**
- The `HIDProtocol::parseCommand()` function (HIDProtocol.cpp:63-81) automatically detects the protocol:
  - Checks for 0xA1 header first
  - Falls back to plain text detection if no header found
  - Plain text validation: First byte must be printable ASCII (0x20-0x7E)
- HID Task displays protocol type in debug output: `[HID CMD 0xA1]` or `[HID CMD 純文本]`

**Response Format:**
- All HID responses use 0xA1 protocol format regardless of input protocol
- Response format: `[0xA1][length][0x00][response_text...]`
- Long responses are automatically split into multiple 64-byte packets by `HIDResponse` class

**Host-Side Implementation:**
- Test script (`test_hid.py`) supports both protocols:
  - `python test_hid.py test` - Plain text protocol
  - `python test_hid.py test-0xa1` - 0xA1 protocol
  - `python test_hid.py interactive` - Interactive mode (plain text)
  - `python test_hid.py interactive-0xa1` - Interactive mode (0xA1 protocol)

### Unified Command System

**CommandParser Architecture:**
The system uses a unified command parser (`CommandParser.h/cpp`) that handles commands from both CDC and HID interfaces:

**Response Interface Pattern:**
- `ICommandResponse`: Abstract interface for command responses
- `CDCResponse`: Implements response through USB serial (`USBSerial`)
  - Format: Plain text with newline (`\n`)
  - Example: `HID_ESP32_S3\n`
- `HIDResponse`: Implements response through HID IN reports
  - Format: `[0xA1][length][0x00][response_text][padding...]`
  - Automatically splits long responses into multiple 64-byte packets
  - Each packet: 3-byte header + up to 61 bytes data
  - Example (2-byte response "OK"): `[0xA1][0x02][0x00]['O']['K'][0x00]...[0x00]`
- `MultiChannelResponse`: Outputs to both CDC and HID simultaneously

**Command Processing Flow:**
1. **CDC Path**: `cdcTask()` → `feedChar()` → `processCommand()` → `CDCResponse` (CDC only)
2. **HID Path**: `hidTask()` → `processCommand()` → `MultiChannelResponse` (CDC + HID)
3. All commands are case-insensitive
4. Commands must end with newline (`\n` or `\r`)
5. **No echo**: Input commands are NOT echoed to the terminal

**Response Routing:**

| Command Source | CDC Response | HID Response |
|---------------|-------------|-------------|
| CDC           | ✓ Yes       | ✗ No        |
| HID           | ✓ Yes       | ✓ Yes       |

**Available Commands:**
- `*IDN?` - SCPI standard identification (returns device ID only, no command echo)
- `HELP` - Show command list
- `INFO` - Device information
- `STATUS` - System status (uptime, free memory, HID data status)
- `SEND` - Send test HID IN report (0x00-0x3F pattern)
- `READ` - Display HID OUT buffer contents in hex dump format
- `CLEAR` - Clear HID OUT buffer

**Adding New Commands:**
1. Add command handler method in `CommandParser` class (e.g., `handleNewCommand()`)
2. Add case in `processCommand()` to detect command string
3. Use `response->print()`, `response->println()`, or `response->printf()` to output
4. Response automatically routes to correct interface based on command source

## HID 64-Byte Limitation and Solution

### Problem Background

During development, we encountered a critical issue where HID OUT reports consistently lost the last byte:
- Host sends: 65 bytes (1 Report ID + 64 data bytes)
- ESP32 receives: Only 63 bytes (missing the last data byte)

### Root Cause Analysis

**USB Packet Size Constraint:**
- USB Full Speed Interrupt endpoints: Maximum 64 bytes per packet
- Report with ID: 1 byte Report ID + 64 bytes data = **65 bytes total**
- Must split into 2 USB packets: 64 bytes + 1 byte

**TinyUSB Implementation Limitation:**
- TinyUSB's HID driver (`CFG_TUD_HID_EP_BUFSIZE`) defaults to 64 bytes
- When data spans multiple USB packets, only the first packet (64 bytes) is processed
- Result: Report ID (1 byte) + First 63 data bytes received, last byte lost

**Verification:**
- Bus Hound USB analyzer confirmed 65 bytes sent from host
- ESP32 debug output showed only 63 bytes received
- Increasing `CFG_TUD_HID_EP_BUFSIZE` to 128 did not solve the issue

### Solution: Remove Report ID

The implemented solution removes the Report ID from the HID descriptor:

**Before (with Report ID):**
```
HID_REPORT_ID(1)
Host sends: [Report ID=0x01] [Data: 64 bytes]
USB wire: Packet 1 [64 bytes] + Packet 2 [1 byte]
ESP32 receives: 63 bytes (last byte lost)
```

**After (without Report ID):**
```
No Report ID field in descriptor
Host sends: [Data: 64 bytes]
USB wire: Single packet [64 bytes]
ESP32 receives: Full 64 bytes ✓
```

**Implementation Details:**
1. Removed `HID_REPORT_ID(1)` from report descriptor (`CustomHID.cpp`)
2. Updated descriptor length from 34 to 32 bytes
3. Modified `send()` to use Report ID = 0 (indicates no Report ID)
4. Updated `_onOutput()` to handle data without Report ID prefix

**Host-Side Changes:**
- Windows: Send 64 bytes directly without Report ID prefix
- Example using pywinusb: `output_report.set_raw_data([0] + data_64_bytes)`
- Example using hidapitester: Omit the `0x01` Report ID from command

### Testing and Verification

Verified with test script sending `0x00` to `0x3F` (64 bytes):
```
ESP32 Output:
[DEBUG] report_id=0x00, raw_len=64
HID OUT 接收: 64 位元組
前16: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
後16: 30 31 32 33 34 35 36 37 38 39 3A 3B 3C 3D 3E 3F ✓
```

All 64 bytes received successfully, including the previously lost `0x3F`.

## FreeRTOS Configuration

### Task Priorities
The system uses priority-based scheduling:
- **HID Task**: Priority 2 (Higher) - Ensures HID data is processed promptly
- **CDC Task**: Priority 1 (Lower) - Can be preempted by HID task
- **IDLE Task**: Priority 0 (Lowest) - Arduino `loop()` function

### Memory Allocation
- Each task has 4KB stack (configurable via `xTaskCreatePinnedToCore`)
- Queue holds 10 HID packets (70 bytes each = 700 bytes total)
- Mutexes have minimal overhead (~100 bytes each)

### Modifying Task Behavior

**Adjusting Task Priority:**
Change the priority parameter in `xTaskCreatePinnedToCore()` (main.cpp:213, 223):
```cpp
xTaskCreatePinnedToCore(hidTask, "HID_Task", 4096, NULL, 2, NULL, 1);  // Priority = 2
```

**Adjusting Stack Size:**
Increase if stack overflow occurs (monitor with `uxTaskGetStackHighWaterMark()`):
```cpp
xTaskCreatePinnedToCore(hidTask, "HID_Task", 8192, NULL, 2, NULL, 1);  // 8KB stack
```

**Adjusting Queue Depth:**
Increase if HID data is lost during bursts (main.cpp:203):
```cpp
hidDataQueue = xQueueCreate(20, sizeof(HIDDataPacket));  // 20 packets
```

### Adding New Tasks
To add a new task:
1. Define task function: `void myTask(void* parameter) { while(true) { ... } }`
2. Create task in `setup()`: `xTaskCreatePinnedToCore(myTask, "My_Task", 4096, NULL, 1, NULL, 1);`
3. Use mutexes to protect shared resources

## Debugging

**CDC Console Debugging:**
- **No character echo**: The system does NOT echo input characters to maintain clean output
- Input commands are processed silently; only responses are displayed
- Character-level debugging can be re-enabled by modifying `CommandParser::feedChar()` if needed

**HID Data Flow Debugging:**
- HID Task automatically prints debug info for received data
- Debug info includes: `report_id`, `raw_len`, and hex dump of data boundaries
- `READ` command displays full 64-byte buffer in hex dump format
- Global flags track HID state:
  - `hid_data_ready`: Persistent flag indicating valid data in buffer
  - Shared buffer protected by `bufferMutex`

**Command Parser Debugging:**
- HID-originated commands display `[HID CMD]` prefix in CDC console
- Helps distinguish command source when debugging dual-interface behavior
- Response routing can be verified by checking which interface receives output:
  - CDC commands: Only CDC receives response
  - HID commands: Both CDC and HID receive response

## Future Development Areas

- **HID Protocol Design**: Implement application-layer protocol in HID Task
  - Use first byte as command/packet type identifier
  - Define data structures for different command types
  - Add CRC/checksum for data integrity
  - Consider separate task for protocol processing

- **HID IN Report Generation**: Create data formatting functions
  - Wrap `HID.send()` with higher-level functions
  - Implement response packet structures
  - Add error status reporting
  - Use queue for outgoing HID data if needed

- **CDC Command Expansion**: Add new commands in `processCommand()`
  - Currently available: help, info, send, status, read, clear
  - Consider adding: config, reset, version, test modes, task stats

- **FreeRTOS Enhancements**:
  - Add task statistics monitoring (`vTaskGetRunTimeStats()`)
  - Implement task watchdog for deadlock detection
  - Add event groups for complex synchronization
  - Consider stream buffers for high-throughput data

- **Performance Optimization**:
  - Profile task execution time and stack usage
  - Optimize queue/mutex timeout values
  - Consider direct-to-task notifications instead of queues
  - Monitor ISR latency with timing measurements

## Host-Side Tools

Example Python test scripts are available in the `test_hid_no_reportid.py` file, demonstrating:
- Device enumeration and opening
- Sending 64-byte HID OUT reports without Report ID
- Structured test patterns for verification

For Windows development:
- Use `pywinusb` library for HID communication
- Use Bus Hound for USB protocol analysis
- Device shows as "TinyUSB HID" in Device Manager

## Troubleshooting

### Device Not Enumerated / Windows Keeps Resetting Device

**Symptoms:**
- Yellow exclamation mark in Device Manager
- Device repeatedly disconnects and reconnects
- `hidapitester --list` doesn't show the device

**Causes and Solutions:**
1. **Incorrect Report Descriptor Length**
   - Verify `CUSTOM_HID_REPORT_DESC_64_LEN` matches actual descriptor size
   - Current correct value: 32 bytes (without Report ID)
   - Use Bus Hound to verify the descriptor is complete

2. **Incomplete USB Descriptor**
   - Check that HID Report Descriptor includes `HID_COLLECTION_END`
   - Verify no bytes are truncated during transmission

3. **Build Configuration Mismatch**
   - Clean build: `pio run -t clean && pio run`
   - Verify build flags are applied (check compiler output for warnings)
   - After upload, physically disconnect and reconnect USB cable

### Only Receiving 63 Bytes Instead of 64

**This is the exact problem this project solves!** If you see this issue:
- Verify the Report Descriptor does **not** include `HID_REPORT_ID()`
- Check `HID.send()` uses Report ID = 0
- Ensure host-side code sends 64 bytes **without** Report ID prefix
- See "HID 64-Byte Limitation and Solution" section above

### CDC Serial Port Not Working

**Symptoms:**
- HID works but no serial console output
- Serial monitor shows no data

**Solutions:**
- Enable DTR (Data Terminal Ready) in your terminal program
- Wait for 5-second timeout after device boot
- Check that `ARDUINO_USB_CDC_ON_BOOT=1` is set in build flags
- Verify correct COM port in Device Manager

### Build Warnings About CONFIG_TINYUSB_HID_BUFSIZE

**Message:** `warning: "CONFIG_TINYUSB_HID_BUFSIZE" redefined`

**This is expected behavior:**
- Our build flags define it as 128
- SDK's sdkconfig.h tries to redefine it as 64
- **Our value (128) takes precedence** - this is correct
- The warnings can be safely ignored

## Documentation

The project documentation is organized as follows:

### For Users and Developers

- **[README.md](README.md)** - Project overview, quick start guide, and basic usage
  - System requirements and hardware/software setup
  - Compilation and upload instructions
  - Quick testing guide
  - Architecture overview
  - Common troubleshooting

- **[PROTOCOL.md](PROTOCOL.md)** - Detailed HID protocol specification
  - Packet format definitions (0xA1 and plain text protocols)
  - Command system architecture
  - Response routing mechanisms
  - FreeRTOS implementation details
  - Protocol version history

- **[TESTING.md](TESTING.md)** - Complete testing guide
  - Test script usage (test_hid.py, test_cdc.py, test_all.py)
  - COM port filtering strategy
  - Testing scenarios and verification procedures
  - Advanced testing techniques
  - Comprehensive troubleshooting

### For AI Assistants

- **[CLAUDE.md](CLAUDE.md)** - This file - AI-assisted development guide
  - Critical configuration details
  - Code structure and architecture
  - Build commands and workflows
  - Implementation decisions and rationale

### Quick Reference

| I want to... | Read this file |
|-------------|---------------|
| Get started quickly | README.md |
| Understand HID protocol | PROTOCOL.md |
| Run tests and verify | TESTING.md |
| Develop and modify code | CLAUDE.md + source code |
