/*
 * TI-84 Claude Bridge - Step 4: Multi-network, multi-profile
 *
 * Boot:
 *   1. Init storage
 *   2. Bootstrap default profile if none exists
 *   3. If saved networks exist, scan + connect to highest priority reachable one
 *   4. Else fall through to AP mode
 */

#include <Arduino.h>
#include "storage.h"
#include "wifi_manager.h"
#include "captive_portal.h"
#include "serial_console.h"
#include "claude_api.h"

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("TI-84 Claude Bridge - Step 4");
    Serial.println("====================================");

    if (!storage.begin()) {
        Serial.println("[FATAL] Storage init failed");
        while (true) delay(1000);
    }

    storage.ensureBootstrap();
    storage.dumpToSerial();

    bool online = false;
    if (storage.getNetworkCount() > 0) {
        Serial.println("[Boot] Attempting station mode...");
        online = wifiManager.connectStation(30000);
    } else {
        Serial.println("[Boot] No saved networks");
    }

    if (online) {
        Serial.println("[Boot] Online. Web UI in STA mode.");
        portal.beginSTA();
    } else {
        Serial.println("[Boot] Starting AP mode for setup.");
        wifiManager.startAccessPoint();
        portal.beginAP();
        Serial.println();
        Serial.println("  ===========================================");
        Serial.println("  Connect to WiFi network:");
        Serial.printf ("    %s\n", WifiManager::AP_SSID);
        Serial.println("  Open: http://192.168.4.1");
        Serial.println("  ===========================================");
        Serial.println();
    }

    console.begin();
}

void loop() {
    portal.poll();
    console.poll();
    delay(2);
}
