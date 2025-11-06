#include "WiFiSettings.h"

const char* WiFiSettingsManager::NVS_NAMESPACE = "wifi";

WiFiSettingsManager::WiFiSettingsManager() {
    // Initialize with defaults
    reset();
}

bool WiFiSettingsManager::begin() {
    // Try to load settings from NVS
    if (!load()) {
        Serial.println("⚠️ WiFi settings not found in NVS, using defaults");
        // Save defaults to NVS
        return save();
    }
    return true;
}

bool WiFiSettingsManager::load() {
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // true = read-only
        Serial.println("❌ Failed to open WiFi settings namespace");
        return false;
    }

    // Check if settings exist
    if (!prefs.isKey("mode")) {
        prefs.end();
        return false;
    }

    // Load WiFi mode
    settings.mode = static_cast<WiFiMode>(prefs.getUChar("mode", static_cast<uint8_t>(WiFiDefaults::MODE)));

    // Load AP settings
    prefs.getString("ap_ssid", settings.ap_ssid, sizeof(settings.ap_ssid));
    prefs.getString("ap_pass", settings.ap_password, sizeof(settings.ap_password));
    settings.ap_channel = prefs.getUChar("ap_chan", WiFiDefaults::AP_CHANNEL);

    // Load Station settings
    prefs.getString("sta_ssid", settings.sta_ssid, sizeof(settings.sta_ssid));
    prefs.getString("sta_pass", settings.sta_password, sizeof(settings.sta_password));
    settings.sta_dhcp = prefs.getBool("sta_dhcp", WiFiDefaults::STA_DHCP);
    prefs.getString("sta_ip", settings.sta_ip, sizeof(settings.sta_ip));
    prefs.getString("sta_gw", settings.sta_gateway, sizeof(settings.sta_gateway));
    prefs.getString("sta_subnet", settings.sta_subnet, sizeof(settings.sta_subnet));

    // Load Web server settings
    settings.web_port = prefs.getUShort("web_port", WiFiDefaults::WEB_PORT);
    settings.web_auth_enabled = prefs.getBool("web_auth", WiFiDefaults::WEB_AUTH_ENABLED);
    prefs.getString("web_user", settings.web_username, sizeof(settings.web_username));
    prefs.getString("web_pass", settings.web_password, sizeof(settings.web_password));

    prefs.end();

    Serial.println("✅ WiFi settings loaded from NVS");
    return true;
}

bool WiFiSettingsManager::save() {
    if (!prefs.begin(NVS_NAMESPACE, false)) {  // false = read-write
        Serial.println("❌ Failed to open WiFi settings namespace for writing");
        return false;
    }

    // Save WiFi mode
    prefs.putUChar("mode", static_cast<uint8_t>(settings.mode));

    // Save AP settings
    prefs.putString("ap_ssid", settings.ap_ssid);
    prefs.putString("ap_pass", settings.ap_password);
    prefs.putUChar("ap_chan", settings.ap_channel);

    // Save Station settings
    prefs.putString("sta_ssid", settings.sta_ssid);
    prefs.putString("sta_pass", settings.sta_password);
    prefs.putBool("sta_dhcp", settings.sta_dhcp);
    prefs.putString("sta_ip", settings.sta_ip);
    prefs.putString("sta_gw", settings.sta_gateway);
    prefs.putString("sta_subnet", settings.sta_subnet);

    // Save Web server settings
    prefs.putUShort("web_port", settings.web_port);
    prefs.putBool("web_auth", settings.web_auth_enabled);
    prefs.putString("web_user", settings.web_username);
    prefs.putString("web_pass", settings.web_password);

    prefs.end();

    Serial.println("✅ WiFi settings saved to NVS");
    return true;
}

void WiFiSettingsManager::reset() {
    settings.mode = WiFiDefaults::MODE;

    // AP settings
    strncpy(settings.ap_ssid, WiFiDefaults::AP_SSID, sizeof(settings.ap_ssid) - 1);
    settings.ap_ssid[sizeof(settings.ap_ssid) - 1] = '\0';
    strncpy(settings.ap_password, WiFiDefaults::AP_PASSWORD, sizeof(settings.ap_password) - 1);
    settings.ap_password[sizeof(settings.ap_password) - 1] = '\0';
    settings.ap_channel = WiFiDefaults::AP_CHANNEL;

    // Station settings
    strncpy(settings.sta_ssid, WiFiDefaults::STA_SSID, sizeof(settings.sta_ssid) - 1);
    settings.sta_ssid[sizeof(settings.sta_ssid) - 1] = '\0';
    strncpy(settings.sta_password, WiFiDefaults::STA_PASSWORD, sizeof(settings.sta_password) - 1);
    settings.sta_password[sizeof(settings.sta_password) - 1] = '\0';
    settings.sta_dhcp = WiFiDefaults::STA_DHCP;
    strncpy(settings.sta_ip, WiFiDefaults::STA_IP, sizeof(settings.sta_ip) - 1);
    settings.sta_ip[sizeof(settings.sta_ip) - 1] = '\0';
    strncpy(settings.sta_gateway, WiFiDefaults::STA_GATEWAY, sizeof(settings.sta_gateway) - 1);
    settings.sta_gateway[sizeof(settings.sta_gateway) - 1] = '\0';
    strncpy(settings.sta_subnet, WiFiDefaults::STA_SUBNET, sizeof(settings.sta_subnet) - 1);
    settings.sta_subnet[sizeof(settings.sta_subnet) - 1] = '\0';

    // Web server settings
    settings.web_port = WiFiDefaults::WEB_PORT;
    settings.web_auth_enabled = WiFiDefaults::WEB_AUTH_ENABLED;
    strncpy(settings.web_username, WiFiDefaults::WEB_USERNAME, sizeof(settings.web_username) - 1);
    settings.web_username[sizeof(settings.web_username) - 1] = '\0';
    strncpy(settings.web_password, WiFiDefaults::WEB_PASSWORD, sizeof(settings.web_password) - 1);
    settings.web_password[sizeof(settings.web_password) - 1] = '\0';

    Serial.println("✅ WiFi settings reset to defaults");
}

const WiFiSettings& WiFiSettingsManager::get() const {
    return settings;
}

WiFiSettings& WiFiSettingsManager::get() {
    return settings;
}
