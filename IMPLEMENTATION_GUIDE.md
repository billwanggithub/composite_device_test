# ESP32-S3 Motor Control System - Implementation Guide

**Firmware Version:** 1.0.0  
**Target Platform:** ESP32-S3-DevKitC-1  
**Date:** November 6, 2025

This guide provides detailed implementation information for AI assistants and developers working with the ESP32-S3 Motor Control System.

---

## Table of Contents

1. [WiFi Connection](#1-wifi-connection)
2. [Command Handling](#2-command-handling)
3. [Web Server](#3-web-server)
4. [PWM Output](#4-pwm-output)
5. [Tachometer Input](#5-tachometer-input)
6. [RPM Calculation](#6-rpm-calculation)

---

## 1. WiFi Connection

### Overview
The system supports three WiFi modes:
- **Station Mode (STA)**: Connects to existing WiFi network
- **Access Point Mode (AP)**: Creates its own WiFi hotspot
- **AP+STA Mode**: Simultaneous AP and Station operation

### Key Components

#### WiFi Credentials Storage
```cpp
struct MotorSettings {
    String wifiSSID = "";
    String wifiPassword = "";
    bool apModeEnabled = DEFAULT_AP_MODE_ENABLED;
};
```

Credentials are persisted in NVS (Non-Volatile Storage) using the `Preferences` library:
```cpp
preferences.putString("wifiSSID", motorSettings.wifiSSID);
preferences.putString("wifiPass", motorSettings.wifiPassword);
preferences.putBool("apModeEn", motorSettings.apModeEnabled);
```

### Implementation Details

#### 1.1 Access Point (AP) Mode

**Function:** `setupWiFiAP()`

**Configuration:**
- SSID: `ESP32_Motor_Control`
- Password: `12345678`
- IP Address: `192.168.4.1`
- Subnet: `255.255.255.0`
- DHCP Range: `192.168.4.2 - 192.168.4.5`
- Max Clients: 4

**Key Features:**
- **Captive Portal**: DNS server redirects all requests to `192.168.4.1`
- **Auto-detection**: Android devices automatically open control page
- **Event Handling**: Monitors client connections/disconnections

**Code Example:**
```cpp
void setupWiFiAP() {
    // Configure IP address
    WiFi.softAPConfig(AP_DEFAULT_IP, AP_GATEWAY, AP_SUBNET);
    
    // Start AP with max 4 clients
    bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 4);
    
    if (apStarted) {
        apModeActive = true;
        
        // Start DNS server for Captive Portal
        const byte DNS_PORT = 53;
        dnsServer.start(DNS_PORT, "*", AP_DEFAULT_IP);
        
        // Set status LED to purple (AP mode indicator)
        setStatusLED(128, 0, 128);
    }
}
```

**Stopping AP Mode:**
```cpp
void stopWiFiAP() {
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    apModeActive = false;
}
```

#### 1.2 Station (STA) Mode

**Function:** `connectToWiFi(String ssid, String password)`

**Connection Process:**
1. Set WiFi mode (WIFI_STA or WIFI_AP_STA)
2. Begin connection with credentials
3. Wait up to 15 seconds for connection (30 attempts × 500ms)
4. Update system state based on result

**Code Example:**
```cpp
void connectToWiFi(String ssid, String password) {
    // Set mode based on AP status
    if (apModeActive) {
        WiFi.mode(WIFI_AP_STA);  // Dual mode
    } else {
        WiFi.mode(WIFI_STA);     // Station only
    }
    
    WiFi.begin(ssid.c_str(), password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        attempts++;
        
        // Allow BLE processing during connection
        if (attempts % 10 == 0) {
            bleProvisioning.loop();
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        currentSystemState = SYSTEM_WIFI_CONNECTED;
        
        // Notify BLE of successful connection
        bleProvisioning.setConnectionStatus(true);
    } else {
        bleProvisioning.setConnectionStatus(false, "Connection failed");
        currentSystemState = SYSTEM_BLE_PROVISIONING;
    }
}
```

#### 1.3 BLE Provisioning Integration

WiFi credentials can be received via BLE:

```cpp
void onBLECredentialsReceived(String ssid, String password) {
    currentSystemState = SYSTEM_WIFI_CONNECTING;
    setupWiFiFromBLE(ssid, password);
}
```

#### 1.4 WiFi Event Handling

Monitor AP client connections:
```cpp
void WiFiAPEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch(event) {
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            // Device connected - ensure web server is running
            if (!webServerStarted) {
                setupWebServer();
            }
            break;
            
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            // Device disconnected
            break;
    }
}
```

### Status LED Indicators

| Color | Status |
|-------|--------|
| Blue (blinking, 1s) | BLE Provisioning mode |
| Yellow (blinking, 0.2s) | WiFi connecting |
| Purple (solid) | AP mode active |
| Green (solid) | WiFi connected and running |
| Red (blinking, 0.5s) | WiFi disconnected |

---

## 2. Command Handling

### Overview
The system supports commands from multiple interfaces:
- **CDC Console**: Serial commands via USB
- **BLE**: Bluetooth Low Energy commands
- **Web API**: RESTful HTTP endpoints

### 2.1 CDC Console Commands

**Function:** `processConsoleCommand(String command)`

**Command Categories:**

#### Information Commands
```
help                  - Display all available commands
status | show        - Display complete system status
ip                   - Show IP address and network info
```

#### PWM Control Commands
```
set pwm_freq <Hz>           - Set PWM frequency (10-500000 Hz)
set pwm_duty <%>            - Set PWM duty cycle (0-100%)
```

#### Configuration Commands
```
set led_brightness <0-255>  - Set RGB LED brightness
set pole_pairs <1-12>       - Set motor pole pairs
set max_freq <Hz>           - Set maximum frequency limit
set wifi_ssid <ssid>        - Set WiFi SSID
set wifi_password <pass>    - Set WiFi password
```

#### Network Commands
```
wifi <ssid> <password>      - Connect to WiFi immediately
wifi connect                - Connect using saved credentials
start_web                   - Start web server manually
ap on                       - Enable Access Point mode
ap off                      - Disable Access Point mode
ap status                   - Show AP status
```

#### Storage Commands
```
save                        - Save all settings to EEPROM
load                        - Load settings from EEPROM
reset                       - Reset to factory defaults
```

### Implementation Details

#### Command Parser
```cpp
void processConsoleCommand(String command) {
    command.trim();
    command.toLowerCase();
    
    if (command.length() == 0) return;
    
    // Single word commands
    if (command == "help") {
        printConsoleHelp();
        return;
    }
    
    if (command == "status" || command == "show") {
        printSystemSettings();
        return;
    }
    
    // Commands with parameters
    if (command.startsWith("set ")) {
        String params = command.substring(4);
        int spaceIndex = params.indexOf(' ');
        
        if (spaceIndex > 0) {
            String parameter = params.substring(0, spaceIndex);
            String value = params.substring(spaceIndex + 1);
            
            // Process parameter
            if (parameter == "pwm_freq") {
                uint32_t freq = value.toInt();
                if (freq >= 10 && freq <= 500000) {
                    setPWMFrequency(freq);
                }
            }
            // ... other parameters
        }
    }
}
```

#### Command Validation
All commands include validation:
- **Range checking**: Ensure values are within acceptable limits
- **Error messages**: Clear feedback on invalid input
- **Hints**: Suggestions for correct usage

**Example:**
```cpp
if (frequency >= 10 && frequency <= 500000) {
    setPWMFrequency(frequency);
    Serial.printf("✅ PWM frequency set to: %d Hz\n", frequency);
} else {
    Serial.println("❌ Error: Frequency must be between 10 and 500000 Hz");
}
```

### 2.2 BLE Command Handling

BLE commands are handled by the `ble_provisioning` module:

**Key Commands:**
- `WIFI <ssid> <password>`: Provide WiFi credentials
- `START_WEB`: Start web server after WiFi connection
- `STATUS`: Get system status
- `VER`: Get firmware version

**Callback System:**
```cpp
bleProvisioning.setOnCredentialsReceived(onBLECredentialsReceived);
bleProvisioning.setOnProvisioningComplete(onBLEProvisioningComplete);
bleProvisioning.setOnWebServerStart(onBLEWebServerStart);
```

### 2.3 Web API Commands

See [Section 3.3](#33-rest-api-endpoints) for HTTP API endpoints.

---

## 3. Web Server

### Overview
The web server provides:
- Web dashboard interface (HTML/CSS/JavaScript)
- RESTful API for motor control
- Real-time status monitoring
- Captive Portal support for mobile devices

**Library:** ESP32 WebServer  
**Port:** 80 (HTTP)  
**File System:** SPIFFS

### 3.1 Server Initialization

**Function:** `setupWebServer()`

**Initialization Steps:**
1. Verify SPIFFS is mounted
2. Register API endpoints
3. Register static file handlers
4. Start server
5. Set `webServerStarted = true`

```cpp
void setupWebServer() {
    // Debug SPIFFS contents
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
        Serial.printf("  Found file: %s (size: %d bytes)\n", 
                     file.name(), file.size());
        file = root.openNextFile();
    }
    
    // Register endpoints...
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleAPIStatus);
    // ... more endpoints
    
    server.begin();
    webServerStarted = true;
}
```

### 3.2 Captive Portal Implementation

**Purpose:** Auto-redirect mobile devices to control page

**Supported Detection URLs:**

| Platform | URL |
|----------|-----|
| Android | `/generate_204`, `/gen_204` |
| Google | `/connectivitycheck.gstatic.com/generate_204` |
| Android 7+ | `/mobile/status.php` |
| Apple iOS | `/hotspot-detect.html`, `/library/test/success.html` |
| Windows | `/ncsi.txt` |

**Implementation:**
```cpp
// Android Captive Portal detection
server.on("/generate_204", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta http-equiv='refresh' content='0;url=http://192.168.4.1/'>";
    html += "</head><body>Redirecting...</body></html>";
    server.send(200, "text/html", html);
});
```

**DNS Server:**
```cpp
// Redirect all DNS queries to AP IP
dnsServer.start(53, "*", AP_DEFAULT_IP);

// In main loop:
if (apModeActive) {
    dnsServer.processNextRequest();
}
```

### 3.3 REST API Endpoints

#### GET Endpoints

##### `/api/status` - System Status
**Response:**
```json
{
    "frequency": 10000,
    "duty": 50.0,
    "polePairs": 2,
    "maxFrequency": 100000,
    "rpm": 12450.5,
    "realInputFrequency": 415.0,
    "uptime": "1:23:45",
    "wifiConnected": true,
    "wifiIP": "192.168.1.100",
    "bleConnected": false,
    "freeHeap": 234560,
    "title": "GMT Motor Control",
    "subtitle": "PWM Control & RPM Monitor",
    "bleDeviceName": "GMT_Motor_BLE",
    "firmwareVersion": "1.0.0",
    "ledBrightness": 25,
    "rpmUpdateRate": 100,
    "apModeEnabled": true,
    "apModeActive": true,
    "apIP": "192.168.4.1"
}
```

**Implementation:**
```cpp
server.on("/api/status", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Content-Type", "application/json");
    String status = getSystemStatus();
    server.send(200, "application/json", status);
});
```

##### `/api/rpm` - RPM Reading
**Response:**
```json
{
    "rpm": 12450.5,
    "realInputFrequency": 415.0,
    "polePairs": 2,
    "frequency": 10000,
    "duty": 50.0,
    "inputFrequency": 415.0
}
```

##### `/api/led-brightness` - Get LED Brightness
**Response:**
```json
{
    "brightness": 25,
    "success": true
}
```

##### `/api/ap-mode` - Get AP Mode Status
**Response:**
```json
{
    "enabled": true,
    "active": true,
    "ip": "192.168.4.1",
    "ssid": "ESP32_Motor_Control",
    "clients": 2,
    "success": true
}
```

#### POST Endpoints

##### `/api/pwm` - Set PWM Parameters
**Parameters:**
- `frequency` (optional): PWM frequency in Hz (10-500000)
- `duty` (optional): Duty cycle percentage (0-100)

**Example Request:**
```
POST /api/pwm
Content-Type: application/x-www-form-urlencoded

frequency=15000&duty=75.5
```

**Response:**
```json
{
    "success": true,
    "message": "Frequency: 15000Hz Duty: 75.5% "
}
```

**Implementation:**
```cpp
server.on("/api/pwm", HTTP_POST, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    bool updated = false;
    String message = "";
    
    if (server.hasArg("frequency")) {
        uint32_t freq = server.arg("frequency").toInt();
        if (freq >= 10 && freq <= 500000) {
            setPWMFrequency(freq);
            updated = true;
            message += "Frequency: " + String(freq) + "Hz ";
        }
    }
    
    if (server.hasArg("duty")) {
        float duty = server.arg("duty").toFloat();
        if (duty >= 0.0 && duty <= 100.0) {
            setPWMDuty(duty);
            updated = true;
            message += "Duty: " + String(duty) + "% ";
        }
    }
    
    if (updated) {
        server.send(200, "application/json", 
                   "{\"success\":true,\"message\":\"" + message + "\"}");
    } else {
        server.send(400, "application/json", 
                   "{\"success\":false,\"message\":\"No valid parameters\"}");
    }
});
```

##### `/api/save` - Save Settings
**Response:**
```json
{
    "success": true,
    "message": "Settings saved to EEPROM"
}
```

##### `/api/load` - Load Settings
**Response:**
```json
{
    "success": true,
    "message": "Settings loaded from EEPROM",
    "frequency": 10000,
    "duty": 50.0,
    "polePairs": 2,
    "maxFrequency": 100000,
    "dutyStepSize": 1.0,
    "freqStepSize": 10
}
```

##### `/api/pole-pairs` - Set Pole Pairs
**Parameters:** `polePairs` (1-12)

##### `/api/max-frequency` - Set Max Frequency
**Parameters:** `maxFrequency` (1000-500000 Hz)

##### `/api/led-brightness` - Set LED Brightness
**Parameters:** `brightness` (0-255)

**Example:**
```
POST /api/led-brightness
Content-Type: application/x-www-form-urlencoded

brightness=128
```

##### `/api/ap-mode` - Control AP Mode
**Parameters:** `enabled` (true/false or 1/0)

**Example:**
```
POST /api/ap-mode
Content-Type: application/x-www-form-urlencoded

enabled=true
```

##### `/api/config` - Update Configuration
**Parameters:** JSON object with configuration fields

**Supported Fields:**
- `title`, `subtitle`, `bleDeviceName`
- `dutyStepSize`, `freqStepSize`
- `language` ("en" or "zh-CN")
- `rpmUpdateRate` (20-1000 ms)
- `maxFrequency` (1000-500000 Hz)
- `polePairs` (1-12)
- `ledBrightness` (0-255)
- `wifiSSID`, `wifiPassword`

### 3.4 Static File Serving

**Root Handler:**
```cpp
server.on("/", HTTP_GET, []() {
    File file = SPIFFS.open("/index.html", "r");
    if (file && file.size() > 0) {
        server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        server.streamFile(file, "text/html");
        file.close();
    } else {
        // Serve fallback HTML
        server.send(200, "text/html", fallbackHTML);
    }
});
```

**Generic File Handler (404):**
```cpp
server.onNotFound([]() {
    String path = server.uri();
    
    if (SPIFFS.exists(path)) {
        File file = SPIFFS.open(path, "r");
        String contentType = "text/plain";
        
        if (path.endsWith(".html")) contentType = "text/html";
        else if (path.endsWith(".css")) contentType = "text/css";
        else if (path.endsWith(".js")) contentType = "application/javascript";
        else if (path.endsWith(".json")) contentType = "application/json";
        
        server.streamFile(file, contentType);
        file.close();
    } else {
        server.send(404, "text/plain", "File Not Found");
    }
});
```

### 3.5 CORS Support

All API endpoints include CORS headers:
```cpp
server.sendHeader("Access-Control-Allow-Origin", "*");
server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
```

OPTIONS handlers for preflight requests:
```cpp
server.on("/api/pwm", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(200);
});
```

---

## 4. PWM Output

### Overview
The system uses the **ESP32-S3 MCPWM (Motor Control PWM)** peripheral for high-precision PWM generation.

**Advantages over LEDC:**
- Smooth frequency switching without glitches
- Higher precision timing
- Hardware-based operation (minimal CPU overhead)

### Hardware Configuration

**Pin:** GPIO 10  
**MCPWM Unit:** MCPWM_UNIT_1  
**Timer:** MCPWM_TIMER_0  
**Generator:** MCPWM0A  
**Base Clock:** 80 MHz (APB clock)  
**Resolution:** 10,000 (0.01% accuracy)

### 4.1 PWM Initialization

**Function:** `setupPWM()`

```cpp
void setupPWM() {
    // Configure GPIO as MCPWM output
    mcpwm_gpio_init(PWM_MCPWM_UNIT, PWM_MCPWM_GEN, PWM_OUTPUT_PIN);
    
    // Configure MCPWM parameters
    mcpwm_config_t pwm_config = {
        .frequency = motorSettings.frequency,    // Initial frequency
        .cmpr_a = 0,                              // Initial duty 0%
        .cmpr_b = 0,                              // Not used
        .duty_mode = MCPWM_DUTY_MODE_0,          // Active high
        .counter_mode = MCPWM_UP_COUNTER,        // Up counter mode
    };
    
    // Initialize MCPWM
    esp_err_t result = mcpwm_init(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, &pwm_config);
    
    if (result == ESP_OK) {
        mcpwmInitialized = true;
        
        // Set initial values
        mcpwm_set_frequency(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, motorSettings.frequency);
        mcpwm_set_duty(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, MCPWM_OPR_A, motorSettings.duty);
        mcpwm_set_duty_type(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    }
}
```

### 4.2 Setting PWM Frequency

**Function:** `setPWMFrequency(uint32_t frequency)`

**Parameters:**
- `frequency`: Desired PWM frequency (10 - 500,000 Hz)

**Key Features:**
- **Change detection**: Skip update if frequency unchanged
- **Glitch-free switching**: MCPWM handles transition smoothly
- **Pulse notification**: Sends pulse on GPIO 12 when changed

```cpp
void setPWMFrequency(uint32_t frequency) {
    // Validate and clamp
    if (frequency < 10) frequency = 10;
    if (frequency > 500000) frequency = 500000;
    
    // Check if MCPWM is initialized
    if (!mcpwmInitialized) {
        Serial.println("❌ MCPWM not initialized!");
        return;
    }
    
    // Skip if unchanged
    uint32_t oldFrequency = motorSettings.frequency;
    if (frequency == oldFrequency) {
        Serial.printf("⏭️  PWM frequency unchanged (%d Hz), skipping\n", frequency);
        return;
    }
    
    motorSettings.frequency = frequency;
    
    // Apply new frequency using MCPWM
    esp_err_t result = mcpwm_set_frequency(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, frequency);
    
    if (result == ESP_OK) {
        Serial.printf("✅ PWM frequency set to: %d Hz (duty: %.1f%%)\n",
                      frequency, motorSettings.duty);
        
        // Send pulse notification
        sendPulseOutput();
    } else {
        Serial.printf("❌ Failed to set PWM frequency: %s\n", 
                     esp_err_to_name(result));
    }
}
```

### 4.3 Setting PWM Duty Cycle

**Function:** `setPWMDuty(float duty)`

**Parameters:**
- `duty`: Duty cycle percentage (0.0 - 100.0%)

**Features:**
- Change detection to avoid unnecessary updates
- Immediate application via MCPWM
- Pulse notification on change

```cpp
void setPWMDuty(float duty) {
    // Validate and clamp
    if (duty < 0.0) duty = 0.0;
    if (duty > 100.0) duty = 100.0;
    
    // Check initialization
    if (!mcpwmInitialized) {
        Serial.println("❌ MCPWM not initialized!");
        return;
    }
    
    // Skip if unchanged (with small tolerance for float comparison)
    if (fabs(duty - motorSettings.duty) < 0.01) {
        Serial.printf("⏭️  PWM duty unchanged (%.1f%%), skipping\n", duty);
        return;
    }
    
    motorSettings.duty = duty;
    
    // Apply new duty cycle
    esp_err_t result = mcpwm_set_duty(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, 
                                      MCPWM_OPR_A, duty);
    
    if (result == ESP_OK) {
        // Ensure duty is applied
        mcpwm_set_duty_type(PWM_MCPWM_UNIT, PWM_MCPWM_TIMER, 
                           MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
        
        Serial.printf("✅ PWM duty set to: %.1f%% (freq: %d Hz)\n",
                      duty, motorSettings.frequency);
        
        // Send pulse notification
        sendPulseOutput();
    } else {
        Serial.printf("❌ Failed to set PWM duty: %s\n", 
                     esp_err_to_name(result));
    }
}
```

### 4.4 Pulse Output Notification

**Purpose:** Signal external systems when PWM changes

**Pin:** GPIO 12  
**Pulse Width:** Configurable (default 10 µs)

```cpp
void sendPulseOutput() {
    digitalWrite(PULSE_OUTPUT_PIN, HIGH);
    delayMicroseconds(motorSettings.pulseWidthUs);
    digitalWrite(PULSE_OUTPUT_PIN, LOW);
}
```

### 4.5 PWM Frequency Range and Resolution

| Frequency Range | Typical Use Case |
|----------------|------------------|
| 10 Hz - 100 Hz | Low-speed motor control |
| 100 Hz - 1 kHz | Servo control |
| 1 kHz - 20 kHz | Standard motor control |
| 20 kHz - 100 kHz | High-frequency motor control |
| 100 kHz - 500 kHz | Special applications |

**Duty Cycle Resolution:**
- 0.01% precision (10,000 steps)
- Smooth control across entire range

---

## 5. Tachometer Input

### Overview
The system uses the **ESP32-S3 MCPWM Capture** peripheral to measure tachometer signals with hardware precision.

**Advantages:**
- Hardware-based timing (no CPU overhead)
- Microsecond-level accuracy
- Immune to CPU load variations
- Interrupt-driven operation

### Hardware Configuration

**Pin:** GPIO 11  
**MCPWM Unit:** MCPWM_UNIT_0  
**Capture Channel:** MCPWM_CAP0  
**Timer Clock:** 80 MHz (APB clock)  
**Edge Detection:** Rising edge (positive edge trigger)

### 5.1 Tachometer Initialization

**Function:** `setupRPMCounter()`

```cpp
void setupRPMCounter() {
    // Configure GPIO as MCPWM Capture input
    mcpwm_gpio_init(MCPWM_UNIT, MCPWM_CAP_0, RPM_INPUT_PIN);
    
    // Setup capture configuration
    mcpwm_capture_config_t cap_conf = {
        .cap_edge = MCPWM_POS_EDGE,           // Capture on rising edge
        .cap_prescale = 1,                     // No prescaling (80 MHz)
        .capture_cb = mcpwm_capture_callback, // Callback function
        .user_data = NULL
    };
    
    // Enable capture channel
    esp_err_t ret = mcpwm_capture_enable_channel(MCPWM_UNIT, 
                                                  MCPWM_CAP_CHANNEL, 
                                                  &cap_conf);
    
    if (ret == ESP_OK) {
        Serial.printf("✅ MCPWM Capture initialized (GPIO %d, rising edge, 80 MHz)\n",
                     RPM_INPUT_PIN);
    }
}
```

### 5.2 Capture Interrupt Handler

**Function:** `mcpwm_capture_callback()`

**Runs in ISR context** - must be fast and use `IRAM_ATTR`

```cpp
static bool IRAM_ATTR mcpwm_capture_callback(mcpwm_unit_t mcpwm,
                                              mcpwm_capture_channel_id_t cap_channel,
                                              const cap_event_data_t *edata,
                                              void *user_data) {
    static uint32_t lastCaptureTime = 0;
    uint32_t currentCaptureTime = edata->cap_value;
    
    if (lastCaptureTime != 0) {
        // Calculate period between captures (in timer ticks)
        if (currentCaptureTime > lastCaptureTime) {
            capturePeriod = currentCaptureTime - lastCaptureTime;
        } else {
            // Handle timer overflow
            capturePeriod = (UINT32_MAX - lastCaptureTime) + currentCaptureTime + 1;
        }
        newCaptureAvailable = true;
    }
    lastCaptureTime = currentCaptureTime;
    
    return false;  // Don't wake high-priority task
}
```

**Key Points:**
- Captures timestamp in 80 MHz timer ticks
- Calculates period between consecutive rising edges
- Handles 32-bit timer overflow
- Sets flag for main loop processing

### 5.3 Period Measurement Details

**Timer Resolution:** 12.5 nanoseconds (1 / 80 MHz)

**Measurement Range:**

| Input Frequency | Period (ticks) | Period (ms) |
|----------------|----------------|-------------|
| 1 Hz | 80,000,000 | 1000.0 |
| 10 Hz | 8,000,000 | 100.0 |
| 100 Hz | 800,000 | 10.0 |
| 1 kHz | 80,000 | 1.0 |
| 10 kHz | 8,000 | 0.1 |
| 100 kHz | 800 | 0.01 |

**Overflow Handling:**
The 32-bit counter overflows at:
```
Overflow time = 2^32 / 80,000,000 = 53.687 seconds
```

This is handled in the ISR to ensure continuous measurement.

---

## 6. RPM Calculation

### Overview
RPM is calculated from the tachometer input frequency and motor pole pairs.

**Update Rate:** 100 ms (configurable: 20-1000 ms)  
**Formula:** `RPM = (Input Frequency × 60) / Pole Pairs`

### 6.1 RPM Update Function

**Function:** `updateRPM()`

**Called from main loop every 100ms**

```cpp
void updateRPM() {
    if (newCaptureAvailable) {
        noInterrupts();  // Atomic read
        uint32_t period = capturePeriod;
        newCaptureAvailable = false;
        interrupts();
        
        if (period > 0) {
            // Calculate input frequency from period
            // Frequency = Clock / Period
            currentInputFrequency = (float)MCPWM_CAP_TIMER_CLK / (float)period;
            
            // Calculate RPM from input frequency
            // RPM = (Frequency × 60) / Pole Pairs
            currentRPM = (currentInputFrequency * 60.0) / (float)motorSettings.polePairs;
        } else {
            currentInputFrequency = 0.0;
            currentRPM = 0.0;
        }
    } else {
        // No new capture - check for timeout
        static uint32_t lastCaptureMillis = 0;
        uint32_t now = millis();
        
        if (now - lastCaptureMillis > 1000) {
            // No signal for 1 second - assume stopped
            currentRPM = 0.0;
            currentInputFrequency = 0.0;
        }
    }
}
```

### 6.2 Calculation Details

#### Step 1: Calculate Input Frequency
```cpp
// Timer clock: 80 MHz = 80,000,000 Hz
// Period: Number of timer ticks between edges
float frequency = 80000000.0 / period;
```

**Example:**
- Period = 8,000 ticks
- Frequency = 80,000,000 / 8,000 = 10,000 Hz

#### Step 2: Calculate RPM
```cpp
// RPM formula: (Frequency × 60 seconds) / Pole Pairs
float rpm = (frequency * 60.0) / polePairs;
```

**Example:**
- Input Frequency = 10,000 Hz
- Pole Pairs = 2
- RPM = (10,000 × 60) / 2 = 300,000 RPM

### 6.3 Pole Pairs Configuration

**Definition:** Number of magnetic pole pairs in the motor

**Common Values:**
- **2 pole pairs** (4 poles): Standard brushless motors
- **4 pole pairs** (8 poles): High-torque motors
- **6 pole pairs** (12 poles): Very high-torque motors

**Configuration:**
```cpp
motorSettings.polePairs = 2;  // Default: 2 pole pairs
```

**Effect on RPM:**
- More pole pairs = Lower RPM for same input frequency
- Relationship is inverse: `RPM ∝ 1 / polePairs`

### 6.4 Frequency to RPM Conversion Table

**For 2 Pole Pairs:**

| Input Frequency | RPM |
|----------------|-----|
| 50 Hz | 1,500 |
| 100 Hz | 3,000 |
| 200 Hz | 6,000 |
| 400 Hz | 12,000 |
| 1,000 Hz | 30,000 |
| 10,000 Hz | 300,000 |

**For 4 Pole Pairs:**

| Input Frequency | RPM |
|----------------|-----|
| 50 Hz | 750 |
| 100 Hz | 1,500 |
| 200 Hz | 3,000 |
| 400 Hz | 6,000 |
| 1,000 Hz | 15,000 |
| 10,000 Hz | 150,000 |

### 6.5 Accuracy and Precision

**Timer Accuracy:** ±0.01% (crystal oscillator dependent)

**Measurement Precision:**
- At 1 Hz: ±0.00125% (1 tick in 80M)
- At 10 Hz: ±0.0125%
- At 100 Hz: ±0.125%
- At 1 kHz: ±1.25%

**Practical Accuracy:**
- **Low speeds (< 100 Hz):** Excellent accuracy
- **Medium speeds (100-10k Hz):** Very good accuracy
- **High speeds (> 10k Hz):** Good accuracy, may need averaging

### 6.6 Zero Speed Detection

**Timeout Method:**
```cpp
if (now - lastCaptureMillis > 1000) {
    // No signal for 1 second - motor stopped
    currentRPM = 0.0;
    currentInputFrequency = 0.0;
}
```

**Minimum Detectable Speed:**
- With 1-second timeout: ~15 RPM (for 2 pole pairs)
- Calculated as: `(1 Hz × 60) / 2 = 30 RPM`

### 6.7 RPM Data Access

**Real-time Access:**
```cpp
// Global variables updated by updateRPM()
extern float currentRPM;
extern float currentInputFrequency;
```

**Via Web API:**
```
GET /api/rpm
```

**Via Serial Console:**
```
status
```

### 6.8 Signal Requirements

**Tachometer Signal Specifications:**
- **Voltage:** 3.3V logic levels (0V = LOW, 3.3V = HIGH)
- **Edge Type:** Rising edge (LOW to HIGH transition)
- **Frequency Range:** 1 Hz to 100 kHz
- **Minimum Pulse Width:** 1 µs (recommended > 5 µs)
- **Signal Quality:** Clean, bounce-free edges

**Input Protection:**
- Built-in GPIO protection on ESP32-S3
- External RC filter recommended for noisy environments
- Optional Schmitt trigger for very noisy signals

---

## System Integration

### System States

```cpp
enum SystemState {
    SYSTEM_BLE_PROVISIONING,    // Initial state - waiting for WiFi credentials
    SYSTEM_WIFI_CONNECTING,     // Connecting to WiFi
    SYSTEM_WIFI_CONNECTED,      // Connected but web server not started
    SYSTEM_RUNNING              // Fully operational
};
```

### Main Loop Structure

```cpp
void loop() {
    unsigned long currentTime = millis();
    
    // Process DNS requests (Captive Portal)
    if (apModeActive) {
        dnsServer.processNextRequest();
    }
    
    // Process web server requests
    if (webServerStarted) {
        server.handleClient();
    }
    
    // Process console commands
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        processConsoleCommand(input);
    }
    
    // State machine
    switch (currentSystemState) {
        case SYSTEM_BLE_PROVISIONING:
            bleProvisioning.loop();
            setStatusLEDBlink(0, 0, 255);  // Blue
            break;
            
        case SYSTEM_WIFI_CONNECTING:
            setStatusLEDBlink(255, 255, 0);  // Yellow
            break;
            
        case SYSTEM_WIFI_CONNECTED:
        case SYSTEM_RUNNING:
            bleProvisioning.loop();
            
            // Update RPM every 100ms
            if (currentTime - lastRPMUpdate >= RPM_UPDATE_INTERVAL) {
                updateRPM();
                lastRPMUpdate = currentTime;
            }
            
            setStatusLED(0, 255, 0);  // Green
            break;
    }
    
    delay(10);
}
```

---

## Configuration Storage

### NVS (Non-Volatile Storage)

All settings are persisted using the `Preferences` library:

**Saved Settings:**
```cpp
void saveSettings() {
    preferences.putUInt("frequency", motorSettings.frequency);
    preferences.putFloat("duty", motorSettings.duty);
    preferences.putUChar("polePairs", motorSettings.polePairs);
    preferences.putUInt("maxFreq", motorSettings.maxFrequency);
    preferences.putString("title", motorSettings.title);
    preferences.putString("subtitle", motorSettings.subtitle);
    preferences.putString("bleDeviceName", motorSettings.bleDeviceName);
    preferences.putFloat("dutyStep", motorSettings.dutyStepSize);
    preferences.putUInt("freqStep", motorSettings.freqStepSize);
    preferences.putString("language", motorSettings.language);
    preferences.putUChar("ledBright", motorSettings.ledBrightness);
    preferences.putString("wifiSSID", motorSettings.wifiSSID);
    preferences.putString("wifiPass", motorSettings.wifiPassword);
    preferences.putUInt("rpmUpdate", motorSettings.rpmUpdateRate);
    preferences.putBool("apModeEn", motorSettings.apModeEnabled);
}
```

**Loaded on Boot:**
```cpp
void loadSettings() {
    motorSettings.frequency = preferences.getUInt("frequency", DEFAULT_FREQUENCY);
    motorSettings.duty = preferences.getFloat("duty", DEFAULT_DUTY);
    // ... load all settings with defaults
}
```

---

## Error Handling

### Validation Patterns

All user inputs are validated:

```cpp
// Range validation
if (frequency >= 10 && frequency <= 500000) {
    setPWMFrequency(frequency);
} else {
    server.send(400, "application/json", 
               "{\"success\":false,\"message\":\"Invalid frequency range\"}");
}

// Initialization checks
if (!mcpwmInitialized) {
    Serial.println("❌ MCPWM not initialized!");
    return;
}

// State checks
if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected");
    return;
}
```

### Error Recovery

- **WiFi Failure:** Falls back to BLE provisioning mode
- **MCPWM Failure:** Logs error, prevents further PWM operations
- **SPIFFS Failure:** Serves fallback HTML
- **Capture Timeout:** Sets RPM to zero after 1 second

---

## Performance Characteristics

### Timing

- **PWM Update:** < 1 ms
- **RPM Update:** Every 100 ms (configurable 20-1000 ms)
- **Web Server Response:** < 50 ms (typical)
- **BLE Response:** < 100 ms

### Memory Usage

- **Heap:** ~200-250 KB free (out of ~320 KB)
- **SPIFFS:** Stores web dashboard files
- **NVS:** ~100 bytes for settings

### CPU Load

- **Idle:** ~5%
- **Active (web serving):** ~15-20%
- **RPM measurement:** < 1% (hardware-based)
- **PWM generation:** < 1% (hardware-based)

---

## Quick Reference

### Pin Assignments
```
GPIO 10  - PWM Output
GPIO 11  - Tachometer Input
GPIO 12  - Pulse Output (change notification)
GPIO 48  - RGB Status LED (WS2812)
```

### Default Values
```
Frequency:      10,000 Hz
Duty Cycle:     0%
Pole Pairs:     2
Max Frequency:  100,000 Hz
LED Brightness: 25 (out of 255)
RPM Update:     100 ms
AP Mode:        Enabled
```

### Network Access
```
AP Mode:  192.168.4.1 (SSID: ESP32_Motor_Control)
STA Mode: DHCP assigned (check serial console)
Port:     80 (HTTP)
```

### Key Commands
```
help              - Show all commands
status            - Show system status
set pwm_freq <Hz> - Set frequency
set pwm_duty <%>  - Set duty cycle
wifi connect      - Connect to saved WiFi
save              - Save settings
```

---

## Troubleshooting

### PWM Not Working
1. Check `mcpwmInitialized` flag
2. Verify GPIO 10 is not used elsewhere
3. Check frequency/duty ranges
4. Monitor serial console for errors

### No RPM Reading
1. Verify tachometer signal is 3.3V logic
2. Check GPIO 11 connection
3. Verify signal frequency is 1 Hz - 100 kHz
4. Check pole pairs setting
5. Monitor `currentInputFrequency` variable

### WiFi Connection Fails
1. Verify SSID/password are correct
2. Check signal strength
3. Try AP mode (192.168.4.1)
4. Use `wifi connect` command
5. Check serial console for connection attempts

### Web Server Not Accessible
1. Check `webServerStarted` flag
2. Verify WiFi is connected
3. Try AP mode
4. Check IP address with `ip` command
5. Verify SPIFFS is mounted

---

**Document Version:** 1.0  
**Last Updated:** November 6, 2025  
**Firmware Compatibility:** v1.0.0+
