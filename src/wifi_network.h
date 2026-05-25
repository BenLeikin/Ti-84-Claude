/*
 * wifi_network.h - Saved WiFi network entry
 *
 * Stored as JSON blobs in NVS, one per slot.
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

struct WifiNetwork {
    String ssid;
    String password;

    String toJson() const {
        JsonDocument doc;
        doc["ssid"] = ssid;
        doc["pass"] = password;
        String out;
        serializeJson(doc, out);
        return out;
    }

    bool fromJson(const String& json) {
        JsonDocument doc;
        if (deserializeJson(doc, json)) return false;
        ssid     = doc["ssid"] | "";
        password = doc["pass"] | "";
        return ssid.length() > 0;
    }
};
