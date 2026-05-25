/*
 * wifi_manager.cpp - multi-network connection logic
 *
 * Scans for available APs, then iterates saved networks in priority order
 * and connects to the first one that's both visible and reachable.
 */

#include "wifi_manager.h"
#include "storage.h"
#include "wifi_network.h"
#include <WiFi.h>
#include <ESPmDNS.h>

WifiManager wifiManager;

const char* WifiManager::AP_SSID = "Setup";

bool WifiManager::connectStation(uint32_t totalTimeoutMs) {
    int saved = storage.getNetworkCount();
    if (saved == 0) {
        Serial.println("[WiFi] No saved networks");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    // Scan once to know which saved networks are nearby. This is faster than
    // blindly attempting each saved network in turn.
    Serial.println("[WiFi] Scanning...");
    int found = WiFi.scanNetworks(false, false, false, 300);  // ~3 seconds
    Serial.printf("[WiFi] Found %d networks\n", found);

    uint32_t startedAt = millis();

    for (int i = 0; i < saved; i++) {
        if (millis() - startedAt > totalTimeoutMs) {
            Serial.println("[WiFi] Overall timeout");
            break;
        }

        WifiNetwork net;
        if (!storage.getNetwork(i, net)) continue;

        // Is this saved network in the scan results?
        bool visible = false;
        int rssi = 0;
        for (int j = 0; j < found; j++) {
            if (WiFi.SSID(j) == net.ssid) {
                visible = true;
                rssi = WiFi.RSSI(j);
                break;
            }
        }

        if (!visible) {
            Serial.printf("[WiFi] [%d] '%s' not visible, skipping\n",
                          i, net.ssid.c_str());
            continue;
        }

        Serial.printf("[WiFi] [%d] Trying '%s' (RSSI %d dBm)...\n",
                      i, net.ssid.c_str(), rssi);

        WiFi.begin(net.ssid.c_str(), net.password.c_str());

        uint32_t attemptStart = millis();
        while (WiFi.status() != WL_CONNECTED &&
               millis() - attemptStart < 12000 &&
               millis() - startedAt < totalTimeoutMs) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Connected to '%s'. IP: %s, RSSI: %d dBm\n",
                          net.ssid.c_str(),
                          WiFi.localIP().toString().c_str(),
                          WiFi.RSSI());

            if (MDNS.begin("ti84claude")) {
                MDNS.addService("http", "tcp", 80);
                Serial.println("[WiFi] mDNS: http://ti84claude.local");
            }

            _mode = WifiMode::STATION;
            return true;
        }

        Serial.printf("[WiFi] '%s' connect failed, trying next\n", net.ssid.c_str());
        WiFi.disconnect(true);
        delay(200);
    }

    Serial.println("[WiFi] No saved network reachable");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _mode = WifiMode::NONE;
    return false;
}

void WifiManager::startAccessPoint() {
    Serial.println("[WiFi] Starting access point...");
    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(AP_SSID, nullptr, 1, 0, 4);
    if (!ok) {
        Serial.println("[WiFi] softAP failed");
        _mode = WifiMode::NONE;
        return;
    }
    delay(100);
    Serial.printf("[WiFi] AP started: '%s', IP: %s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());
    _mode = WifiMode::ACCESS_POINT;
}

void WifiManager::stop() {
    MDNS.end();
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    _mode = WifiMode::NONE;
}

String WifiManager::localIP() const {
    if (_mode == WifiMode::STATION)      return WiFi.localIP().toString();
    if (_mode == WifiMode::ACCESS_POINT) return WiFi.softAPIP().toString();
    return "0.0.0.0";
}
