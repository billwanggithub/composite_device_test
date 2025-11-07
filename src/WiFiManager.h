#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "WiFiSettings.h"

/**
 * @brief WiFi connection status
 */
enum class WiFiStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AP_STARTED,
    ERROR
};

/**
 * @brief WiFi Manager
 *
 * Handles WiFi connection management for both AP and Station modes.
 * Supports automatic fallback and reconnection.
 */
class WiFiManager {
public:
    /**
     * @brief Constructor
     */
    WiFiManager();

    /**
     * @brief Initialize WiFi manager
     * @param settings Pointer to WiFi settings
     * @return true if successful
     */
    bool begin(WiFiSettings* settings);

    /**
     * @brief Start WiFi in configured mode
     * @return true if successful
     */
    bool start();

    /**
     * @brief Stop WiFi
     */
    void stop();

    /**
     * @brief Update WiFi status (call periodically)
     *
     * Handles reconnection and status monitoring.
     * Should be called from a FreeRTOS task or loop.
     */
    void update();

    /**
     * @brief Get current WiFi status
     * @return WiFiStatus enum
     */
    WiFiStatus getStatus() const;

    /**
     * @brief Get WiFi mode string
     * @return Mode name
     */
    String getModeString() const;

    /**
     * @brief Get IP address
     * @return IP address string (AP or STA depending on mode)
     */
    String getIPAddress() const;

    /**
     * @brief Get number of connected clients (AP mode only)
     * @return Client count
     */
    uint8_t getClientCount() const;

    /**
     * @brief Get RSSI (Station mode only)
     * @return Signal strength in dBm
     */
    int8_t getRSSI() const;

    /**
     * @brief Check if WiFi is connected
     * @return true if connected (AP started or STA connected)
     */
    bool isConnected() const;

    /**
     * @brief Start Access Point mode
     * @return true if successful
     */
    bool startAP();

    /**
     * @brief Start Station mode
     * @return true if successful
     */
    bool startStation();

    /**
     * @brief Scan for available networks
     * @param maxResults Maximum number of results (default 20)
     * @return Number of networks found
     */
    int scanNetworks(int maxResults = 20);

    /**
     * @brief Get scan result
     * @param index Network index (0 to scanNetworks()-1)
     * @param ssid Output SSID string
     * @param rssi Output RSSI value
     * @param secure Output security status
     * @return true if valid index
     */
    bool getScanResult(int index, String& ssid, int8_t& rssi, bool& secure);

private:
    WiFiSettings* pSettings = nullptr;
    WiFiStatus status = WiFiStatus::DISCONNECTED;
    unsigned long lastReconnectAttempt = 0;
    unsigned long lastStatusCheck = 0;
    int scanResults = -1;

    static const uint32_t RECONNECT_INTERVAL_MS = 5000;  // 5 seconds
    static const uint32_t STATUS_CHECK_INTERVAL_MS = 1000;  // 1 second
    static const uint32_t CONNECTION_TIMEOUT_MS = 10000;  // 10 seconds

    /**
     * @brief Configure static IP (if DHCP disabled)
     * @return true if successful
     */
    bool configureStaticIP();
};

#endif // WIFI_MANAGER_H
