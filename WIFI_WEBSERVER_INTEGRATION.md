# WiFi Web Server Integration Guide - Phase 2

**Status:** Phase 1 Complete (Infrastructure) | Phase 2 Pending (Integration)
**Commit:** 99ce07e - WIP: Add WiFi Web Server infrastructure (Phase 1)

---

## Overview

Phase 1 has created all the necessary WiFi and Web Server modules. Phase 2 involves integrating these modules into the main application (`main.cpp`) and adding command processing to `CommandParser.cpp`.

### Phase 1 - Completed ✅

- ✅ WiFiSettings module (NVS storage)
- ✅ WiFiManager module (connection management)
- ✅ WebServerManager module (HTTP + WebSocket)
- ✅ Responsive HTML/CSS/JS web interface
- ✅ REST API endpoints
- ✅ WebSocket real-time updates
- ✅ Command Parser header declarations

### Phase 2 - TODO 📋

- ⏸️ Add WiFi commands to CommandParser::processCommand()
- ⏸️ Create global WiFi/Web instances in main.cpp
- ⏸️ Initialize WiFi and Web Server in setup()
- ⏸️ Create WiFi FreeRTOS task
- ⏸️ Test and verify

---

## Step 1: Add WiFi Commands to CommandParser.cpp

The WiFi command implementations are ready in `src/WiFiCommands.txt`. They need to be integrated into `CommandParser.cpp`.

### 1.1 Add Command Recognition

In `src/CommandParser.cpp`, find the `processCommand()` function (around line 230, after the RESET command) and add:

```cpp
    // WiFi 狀態
    if (upper == "WIFI STATUS") {
        handleWiFiStatus(response);
        return true;
    }

    // WiFi 啟動
    if (upper == "WIFI START") {
        handleWiFiStart(response);
        return true;
    }

    // WiFi 停止
    if (upper == "WIFI STOP") {
        handleWiFiStop(response);
        return true;
    }

    // WiFi 掃描
    if (upper == "WIFI SCAN") {
        handleWiFiScan(response);
        return true;
    }

    // Web 伺服器狀態
    if (upper == "WEB STATUS") {
        handleWebStatus(response);
        return true;
    }
```

### 1.2 Update HELP Command

In the `handleHelp()` function (around line 340), add:

```cpp
    response->println("");
    response->println("WiFi & Web 伺服器:");
    response->println("  WIFI STATUS  - 顯示 WiFi 連線狀態");
    response->println("  WIFI START   - 啟動 WiFi");
    response->println("  WIFI STOP    - 停止 WiFi");
    response->println("  WIFI SCAN    - 掃描可用網路");
    response->println("  WEB STATUS   - 顯示 Web 伺服器狀態");
```

### 1.3 Add Handler Implementations

At the end of `CommandParser.cpp` (before the HIDResponse/BLEResponse implementations), add the complete handler functions from `src/WiFiCommands.txt`.

---

## Step 2: Integrate into main.cpp

### 2.1 Add Includes

At the top of `src/main.cpp`, add:

```cpp
#include "WiFiSettings.h"
#include "WiFiManager.h"
#include "WebServer.h"
```

### 2.2 Create Global Instances

After the existing global instances (around line 74), add:

```cpp
// WiFi and Web Server instances
WiFiSettingsManager wifiSettingsManager;
WiFiManager wifiManager;
WebServerManager webServerManager;
```

### 2.3 Add WiFi Task Function

Before `setup()` function, add:

```cpp
// WiFi 更新 Task
void wifiTask(void* parameter) {
    TickType_t lastUpdate = 0;
    TickType_t lastWebUpdate = 0;

    while (true) {
        TickType_t now = xTaskGetTickCount();

        // Update WiFi connection status every second
        if (now - lastUpdate >= pdMS_TO_TICKS(1000)) {
            wifiManager.update();
            lastUpdate = now;
        }

        // Update Web Server every 200ms (for WebSocket broadcasts)
        if (now - lastWebUpdate >= pdMS_TO_TICKS(200)) {
            webServerManager.update();
            lastWebUpdate = now;
        }

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 2.4 Initialize in setup()

In the `setup()` function, after motor control initialization (around line 480), add:

```cpp
    // ========== WiFi and Web Server Initialization ==========

    Serial.println("");
    Serial.println("=== 初始化 WiFi 和 Web 伺服器 ===");

    // Initialize WiFi settings
    if (!wifiSettingsManager.begin()) {
        Serial.println("⚠️ WiFi settings initialization failed, using defaults");
    }

    // Initialize WiFi manager
    if (!wifiManager.begin(&wifiSettingsManager.get())) {
        Serial.println("❌ WiFi Manager initialization failed!");
    } else {
        // Start WiFi (based on saved settings)
        if (wifiSettingsManager.get().mode != WiFiMode::OFF) {
            wifiManager.start();
        }
    }

    // Initialize Web Server
    if (!webServerManager.begin(&wifiSettingsManager.get(),
                                 &motorControl,
                                 &motorSettingsManager,
                                 &wifiManager)) {
        Serial.println("❌ Web Server initialization failed!");
    } else {
        // Start web server if WiFi is connected
        if (wifiManager.isConnected()) {
            webServerManager.start();
        }
    }
```

### 2.5 Create WiFi Task

In `setup()`, after creating the motor task (around line 550), add:

```cpp
    // Create WiFi task (Priority 1, Core 1)
    xTaskCreatePinnedToCore(
        wifiTask,
        "WiFi_Task",
        8192,        // 8KB stack (needs more for WiFi/WebServer)
        NULL,
        1,           // Priority 1 (same as CDC/BLE tasks)
        NULL,
        1            // Core 1
    );
    Serial.println("✅ WiFi Task created");
```

### 2.6 Update Welcome Message

In `setup()`, update the welcome message to include WiFi info:

```cpp
    Serial.println("");
    Serial.println("╔════════════════════════════════════════════════╗");
    Serial.println("║  ESP32-S3 Multi-Interface Console (v2.4)      ║");
    Serial.println("║  Motor Control + WiFi Web Server              ║");
    Serial.println("╚════════════════════════════════════════════════╝");
    Serial.println("");
    Serial.println("可用介面:");
    Serial.println("  ✓ USB CDC (Serial Console)");
    Serial.println("  ✓ USB HID (64-byte Custom Protocol)");
    Serial.println("  ✓ BLE GATT (Wireless Console)");

    if (wifiManager.isConnected()) {
        Serial.printf("  ✓ WiFi Web Server: http://%s/\n", wifiManager.getIPAddress().c_str());
    }

    Serial.println("");
    Serial.println("輸入 HELP 查看可用命令");
    Serial.println("輸入 WIFI STATUS 查看 WiFi 狀態");
    Serial.println("══════════════════════════════════════════════════");
    Serial.println("");
```

---

## Step 3: Testing the WiFi Web Server

### 3.1 Build and Upload

```bash
# Clean build
pio run -t clean

# Build
pio run

# Upload (enter bootloader mode first)
pio run -t upload
```

### 3.2 Initial Testing

1. **Serial Console Testing:**
   ```
   > HELP
   # Should show WiFi commands

   > WIFI STATUS
   # Should show WiFi status (default AP mode)

   > WEB STATUS
   # Should show web server status
   ```

2. **Connect to WiFi AP:**
   - SSID: `ESP32-Motor-Control`
   - Password: `12345678`
   - Default IP: `192.168.4.1`

3. **Access Web Interface:**
   - Open browser: `http://192.168.4.1/`
   - Should see motor control dashboard
   - WebSocket should connect (green indicator)

### 3.3 Web Interface Testing

1. **Real-time RPM Display:**
   - Motor should show current RPM
   - Updates every 200ms via WebSocket

2. **Frequency Control:**
   - Enter frequency (e.g., 25000)
   - Click "Set Frequency"
   - PWM frequency should update

3. **Duty Cycle Control:**
   - Drag slider (0-100%)
   - Click "Set Duty Cycle"
   - Motor duty should update

4. **Emergency Stop:**
   - Click "EMERGENCY STOP"
   - Motor should stop immediately

5. **Settings:**
   - Click "Save Settings"
   - Settings saved to NVS

### 3.4 REST API Testing

Use `curl` or Postman to test API endpoints:

```bash
# Get motor status
curl http://192.168.4.1/api/status

# Get settings
curl http://192.168.4.1/api/settings

# Set frequency (15 kHz)
curl -X POST http://192.168.4.1/api/motor/freq -d "value=15000"

# Set duty (50%)
curl -X POST http://192.168.4.1/api/motor/duty -d "value=50"

# Emergency stop
curl -X POST http://192.168.4.1/api/motor/stop

# Save settings
curl -X POST http://192.168.4.1/api/settings/save

# Get WiFi status
curl http://192.168.4.1/api/wifi/status

# Scan networks
curl http://192.168.4.1/api/wifi/scan
```

---

## Step 4: WiFi Configuration

### 4.1 Change to Station Mode

To connect to an existing WiFi network:

```
> WIFI STOP
> # Edit WiFiSettings.h defaults or use NVS to set STA credentials
> WIFI START
```

### 4.2 Configure Static IP

Edit `src/WiFiSettings.h` defaults:

```cpp
const bool STA_DHCP = false;
const char* STA_IP = "192.168.1.100";
const char* STA_GATEWAY = "192.168.1.1";
const char* STA_SUBNET = "255.255.255.0";
```

### 4.3 Change AP Credentials

Edit `src/WiFiSettings.h` defaults:

```cpp
const char* AP_SSID = "MyESP32Motor";
const char* AP_PASSWORD = "MyPassword123";
const uint8_t AP_CHANNEL = 6;
```

---

## Step 5: Advanced Features

### 5.1 Web Authentication

Enable basic authentication in `WiFiSettings.h`:

```cpp
const bool WEB_AUTH_ENABLED = true;
const char* WEB_USERNAME = "admin";
const char* WEB_PASSWORD = "secure123";
```

Then update `WebServer.cpp` to add authentication middleware.

### 5.2 Custom Web Port

Change default port in `WiFiSettings.h`:

```cpp
const uint16_t WEB_PORT = 8080;
```

### 5.3 Multiple WiFi Networks (Future)

Implement WiFi credential storage for multiple networks with auto-selection.

---

## Troubleshooting

### Issue: WiFi Not Starting

**Check:**
1. WiFi mode not set to OFF
2. Credentials configured correctly
3. Serial output for error messages

**Solution:**
```
> WIFI STATUS  # Check current configuration
> WIFI START   # Manually start WiFi
```

### Issue: Can't Connect to AP

**Check:**
1. Correct SSID/Password
2. Channel not congested (try channel 1, 6, or 11)
3. Distance from ESP32

**Solution:**
```
> WIFI SCAN    # Scan to verify ESP32 AP is visible
```

### Issue: WebSocket Not Connecting

**Check:**
1. Web server is running: `WEB STATUS`
2. Browser console for errors
3. Firewall not blocking WebSocket

**Solution:**
- Refresh browser page
- Clear browser cache
- Try different browser

### Issue: Out of Memory

**Symptoms:**
- Random reboots
- Web server crashes
- WiFi disconnects

**Solution:**
- Increase WiFi task stack size (currently 8KB)
- Reduce WebSocket broadcast frequency
- Disable unused features

---

## Memory Usage

### Estimated RAM Usage:

| Component | RAM Usage |
|-----------|-----------|
| WiFi Stack | ~30-40 KB |
| Web Server | ~8-12 KB |
| WebSocket Buffer | ~2-4 KB |
| HTML Page (embedded) | ~15 KB |
| **Total Additional** | **~55-71 KB** |

### Available Memory:

- ESP32-S3: 512 KB SRAM + 8 MB PSRAM
- Current usage (without WiFi): ~100 KB
- After WiFi: ~155-171 KB
- **Plenty of headroom** ✅

---

## Performance Considerations

### Task Priorities:

```
Priority 2: HID Task (highest - real-time)
Priority 1: CDC, BLE, Motor, WiFi Tasks
Priority 0: Arduino loop (idle)
```

### Update Rates:

- Motor RPM: 100ms (10 Hz)
- WebSocket broadcast: 200ms (5 Hz)
- WiFi status check: 1000ms (1 Hz)
- Web server cleanup: 200ms

### Network Latency:

- WebSocket RTT: <50ms (local network)
- REST API response: <100ms
- Real-time RPM lag: ~200-300ms

---

## Security Considerations

### Current Implementation:

- ⚠️ No HTTPS (plain HTTP)
- ⚠️ No authentication by default
- ⚠️ No CORS restrictions
- ⚠️ Open WebSocket

### Recommendations for Production:

1. **Enable Web Authentication:**
   - Set `web_auth_enabled = true`
   - Use strong password

2. **Use HTTPS:**
   - Generate self-signed certificate
   - Update ESPAsyncWebServer for SSL

3. **Restrict Access:**
   - Implement IP whitelist
   - Add CORS headers
   - Token-based authentication

4. **Network Isolation:**
   - Run on isolated VLAN
   - Use firewall rules
   - Disable unnecessary services

---

## Future Enhancements

### Planned Features:

- **OTA Firmware Updates** via web interface
- **Real-time Charts** for RPM visualization
- **Data Logging** to SD card or cloud
- **Email/SMS Alerts** for safety events
- **Multi-user Support** with roles/permissions
- **MQTT Integration** for IoT platforms
- **RESTful API Documentation** (Swagger/OpenAPI)
- **Mobile App** (Flutter/React Native)

### Advanced Web Features:

- WebSocket encryption (WSS)
- Server-Sent Events (SSE) alternative
- Progressive Web App (PWA) support
- Offline mode with service workers
- Real-time graph plotting
- Configuration wizard UI

---

## Testing Checklist

- [ ] WiFi AP mode starts successfully
- [ ] Can connect to ESP32 AP from phone/laptop
- [ ] Web interface loads at http://192.168.4.1/
- [ ] WebSocket connects (green indicator)
- [ ] Real-time RPM updates working
- [ ] Frequency control via web works
- [ ] Duty cycle control via web works
- [ ] Emergency stop works
- [ ] Settings save/load works
- [ ] REST API endpoints respond correctly
- [ ] WiFi scan finds networks
- [ ] Station mode connects to WiFi router
- [ ] Static IP configuration works
- [ ] Commands work via serial (WIFI STATUS, etc.)
- [ ] Multiple WebSocket clients supported
- [ ] Web interface works on mobile devices

---

## Files Modified/Created

### New Files:
- `src/WiFiSettings.h` - WiFi configuration structure
- `src/WiFiSettings.cpp` - NVS storage implementation
- `src/WiFiManager.h` - WiFi connection management
- `src/WiFiManager.cpp` - WiFi implementation
- `src/WebServer.h` - Web server interface
- `src/WebServer.cpp` - Web server + HTML implementation
- `src/WiFiCommands.txt` - Command implementations

### Modified Files:
- `platformio.ini` - Added libraries (ESPAsyncWebServer, AsyncTCP, ArduinoJson)
- `src/CommandParser.h` - Added WiFi command declarations
- `src/CommandParser.cpp` - Added extern declarations
- `src/main.cpp` - (Pending) Integration

---

## Summary

**Phase 1 Status:** ✅ Complete - All infrastructure ready

**Phase 2 Tasks:**
1. Add WiFi commands to CommandParser (10 min)
2. Integrate into main.cpp (20 min)
3. Test WiFi AP mode (10 min)
4. Test web interface (15 min)
5. Test REST API (10 min)

**Total Phase 2 Time:** ~65 minutes

**Result:** Full-featured WiFi web server for remote motor control! 🚀

---

**Next Steps:** Follow Step 1-3 above to complete Phase 2 integration.
