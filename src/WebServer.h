#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include "WiFiSettings.h"
#include "MotorControl.h"
#include "MotorSettings.h"
#include "WiFiManager.h"

/**
 * @brief Web Server Manager
 *
 * Provides HTTP REST API and WebSocket interface for motor control.
 * Includes a web-based GUI for monitoring and control.
 */
class WebServerManager {
public:
    /**
     * @brief Constructor
     */
    WebServerManager();

    /**
     * @brief Initialize web server
     * @param wifiSettings Pointer to WiFi settings
     * @param motorControl Pointer to motor control instance
     * @param motorSettingsManager Pointer to motor settings manager
     * @param wifiManager Pointer to WiFi manager
     * @return true if successful
     */
    bool begin(WiFiSettings* wifiSettings,
               MotorControl* motorControl,
               MotorSettingsManager* motorSettingsManager,
               WiFiManager* wifiManager);

    /**
     * @brief Start web server
     * @return true if successful
     */
    bool start();

    /**
     * @brief Stop web server
     */
    void stop();

    /**
     * @brief Update web server (call periodically)
     *
     * Handles WebSocket broadcasts and cleanup.
     * Should be called from FreeRTOS task or loop.
     */
    void update();

    /**
     * @brief Check if server is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Get number of WebSocket clients
     * @return Client count
     */
    uint32_t getWSClientCount() const;

    /**
     * @brief Broadcast RPM update to all WebSocket clients
     * @param rpm Current RPM value
     */
    void broadcastRPM(float rpm);

    /**
     * @brief Broadcast motor status to all WebSocket clients
     */
    void broadcastStatus();

private:
    AsyncWebServer* server = nullptr;
    AsyncWebSocket* ws = nullptr;

    WiFiSettings* pWiFiSettings = nullptr;
    MotorControl* pMotorControl = nullptr;
    MotorSettingsManager* pMotorSettingsManager = nullptr;
    WiFiManager* pWiFiManager = nullptr;

    bool running = false;
    unsigned long lastWSBroadcast = 0;

    static const uint32_t WS_BROADCAST_INTERVAL_MS = 200;  // 5 Hz updates

    /**
     * @brief Setup HTTP routes
     */
    void setupRoutes();

    /**
     * @brief Setup WebSocket handlers
     */
    void setupWebSocket();

    /**
     * @brief Handle WebSocket event
     */
    void handleWebSocketEvent(AsyncWebSocket *server,
                              AsyncWebSocketClient *client,
                              AwsEventType type,
                              void *arg,
                              uint8_t *data,
                              size_t len);

    /**
     * @brief Handle WebSocket message
     */
    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);

    /**
     * @brief Generate HTML page
     */
    String generateIndexHTML();

    /**
     * @brief Generate JSON status response
     */
    String generateStatusJSON();

    /**
     * @brief Generate JSON settings response
     */
    String generateSettingsJSON();

    // REST API handlers
    void handleGetStatus(AsyncWebServerRequest *request);
    void handleGetSettings(AsyncWebServerRequest *request);
    void handleSetPWMFreq(AsyncWebServerRequest *request);
    void handleSetPWMDuty(AsyncWebServerRequest *request);
    void handleMotorStop(AsyncWebServerRequest *request);
    void handleSaveSettings(AsyncWebServerRequest *request);
    void handleLoadSettings(AsyncWebServerRequest *request);
    void handleResetSettings(AsyncWebServerRequest *request);
    void handleGetWiFiStatus(AsyncWebServerRequest *request);
    void handleScanNetworks(AsyncWebServerRequest *request);

    // New API handlers for clone implementation
    void handleGetRPM(AsyncWebServerRequest *request);
    void handleGetConfig(AsyncWebServerRequest *request);
    void handlePostConfig(AsyncWebServerRequest *request);
    void handlePostPWM(AsyncWebServerRequest *request);
    void handlePostPolePairs(AsyncWebServerRequest *request);
    void handlePostMaxFrequency(AsyncWebServerRequest *request);
    void handleGetAPMode(AsyncWebServerRequest *request);
    void handlePostAPMode(AsyncWebServerRequest *request);
    void handlePostSave(AsyncWebServerRequest *request);
    void handlePostLoad(AsyncWebServerRequest *request);
};

#endif // WEB_SERVER_H
