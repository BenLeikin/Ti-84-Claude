/*
 * wifi_manager.h - Connection management for both AP and STA modes
 *
 * Two modes:
 *   AP mode: ESP32 broadcasts its own network for first-time setup
 *   STA mode: ESP32 connects to the user's saved WiFi
 *
 * At boot, main.cpp checks if credentials exist and picks the right mode.
 * If STA connect fails, falls back to AP mode automatically.
 */

#pragma once

#include <Arduino.h>

enum class WifiMode {
    NONE,
    STATION,
    ACCESS_POINT
};

class WifiManager {
public:
    // Connect to the saved network. Returns true on success.
    bool connectStation(uint32_t timeoutMs = 20000);

    // Start AP mode for captive portal setup.
    // SSID is fixed, no password (intentionally open for ease of use).
    void startAccessPoint();

    // Stop whatever mode is active and shut down the radio.
    void stop();

    WifiMode currentMode() const { return _mode; }

    // The IP address users should hit for the config UI.
    // In AP mode, this is the soft-AP IP (192.168.4.1).
    // In STA mode, this is the DHCP-assigned IP.
    String localIP() const;

    // SSID of the AP we broadcast. Defined in cpp.
    static const char* AP_SSID;

private:
    WifiMode _mode = WifiMode::NONE;
};

extern WifiManager wifiManager;
