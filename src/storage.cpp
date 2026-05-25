/*
 * storage.cpp
 */

#include "storage.h"

Storage storage;

bool Storage::begin() {
    bool a = _wifiNets.begin("wifi_nets", false);
    bool b = _profiles.begin("profiles", false);
    if (!a || !b) {
        Serial.println("[Storage] NVS open failed");
        return false;
    }
    Serial.println("[Storage] NVS initialized");
    return true;
}

void Storage::ensureBootstrap() {
    if (getProfileCount() == 0) {
        Serial.println("[Storage] First boot: creating default profile");
        addProfile(Profile::makeDefault());
        setActiveProfileIndex(0);
    }
}

// =============================================================================
// WiFi networks
// =============================================================================

int Storage::getNetworkCount() {
    return _wifiNets.getUChar("count", 0);
}

bool Storage::getNetwork(int index, WifiNetwork& out) {
    if (index < 0 || index >= getNetworkCount()) return false;
    String json = _wifiNets.getString(wifiKey(index).c_str(), "");
    if (json.length() == 0) return false;
    return out.fromJson(json);
}

bool Storage::addNetwork(const WifiNetwork& net) {
    int count = getNetworkCount();
    if (count >= MAX_NETWORKS) return false;
    _wifiNets.putString(wifiKey(count).c_str(), net.toJson());
    _wifiNets.putUChar("count", count + 1);
    return true;
}

bool Storage::updateNetwork(int index, const WifiNetwork& net) {
    if (index < 0 || index >= getNetworkCount()) return false;
    _wifiNets.putString(wifiKey(index).c_str(), net.toJson());
    return true;
}

bool Storage::deleteNetwork(int index) {
    int count = getNetworkCount();
    if (index < 0 || index >= count) return false;

    // Shift later networks down to fill the gap
    for (int i = index; i < count - 1; i++) {
        String next = _wifiNets.getString(wifiKey(i + 1).c_str(), "");
        _wifiNets.putString(wifiKey(i).c_str(), next);
    }
    _wifiNets.remove(wifiKey(count - 1).c_str());
    _wifiNets.putUChar("count", count - 1);
    return true;
}

bool Storage::moveNetworkUp(int index) {
    if (index <= 0 || index >= getNetworkCount()) return false;
    String a = _wifiNets.getString(wifiKey(index - 1).c_str(), "");
    String b = _wifiNets.getString(wifiKey(index).c_str(), "");
    _wifiNets.putString(wifiKey(index - 1).c_str(), b);
    _wifiNets.putString(wifiKey(index).c_str(), a);
    return true;
}

bool Storage::moveNetworkDown(int index) {
    int count = getNetworkCount();
    if (index < 0 || index >= count - 1) return false;
    return moveNetworkUp(index + 1);
}

// =============================================================================
// Profiles
// =============================================================================

int Storage::getProfileCount() {
    return _profiles.getUChar("count", 0);
}

int Storage::getActiveProfileIndex() {
    int active = _profiles.getUChar("active", 0);
    int count = getProfileCount();
    if (active >= count) active = 0;
    return active;
}

bool Storage::setActiveProfileIndex(int index) {
    if (index < 0 || index >= getProfileCount()) return false;
    _profiles.putUChar("active", index);
    return true;
}

bool Storage::getProfile(int index, Profile& out) {
    if (index < 0 || index >= getProfileCount()) return false;
    String json = _profiles.getString(profileKey(index).c_str(), "");
    if (json.length() == 0) return false;
    return out.fromJson(json);
}

bool Storage::getActiveProfile(Profile& out) {
    return getProfile(getActiveProfileIndex(), out);
}

bool Storage::addProfile(const Profile& p) {
    int count = getProfileCount();
    if (count >= MAX_PROFILES) return false;
    _profiles.putString(profileKey(count).c_str(), p.toJson());
    _profiles.putUChar("count", count + 1);
    return true;
}

bool Storage::updateProfile(int index, const Profile& p) {
    if (index < 0 || index >= getProfileCount()) return false;
    _profiles.putString(profileKey(index).c_str(), p.toJson());
    return true;
}

bool Storage::deleteProfile(int index) {
    int count = getProfileCount();
    if (count <= 1) return false;          // never delete the last profile
    if (index < 0 || index >= count) return false;

    // Shift later profiles down
    for (int i = index; i < count - 1; i++) {
        String next = _profiles.getString(profileKey(i + 1).c_str(), "");
        _profiles.putString(profileKey(i).c_str(), next);
    }
    _profiles.remove(profileKey(count - 1).c_str());
    _profiles.putUChar("count", count - 1);

    // Adjust active index if needed
    int active = getActiveProfileIndex();
    if (active == index) {
        setActiveProfileIndex(0);
    } else if (active > index) {
        setActiveProfileIndex(active - 1);
    }
    return true;
}

// =============================================================================
// Maintenance
// =============================================================================

void Storage::clearAll() {
    _wifiNets.clear();
    _profiles.clear();
    Serial.println("[Storage] All config cleared");
}

void Storage::dumpToSerial() {
    Serial.println();
    Serial.println("=== Saved WiFi Networks ===");
    int nCount = getNetworkCount();
    if (nCount == 0) {
        Serial.println("  (none)");
    } else {
        for (int i = 0; i < nCount; i++) {
            WifiNetwork w;
            if (getNetwork(i, w)) {
                Serial.printf("  [%d] %s\n", i, w.ssid.c_str());
            }
        }
    }

    Serial.println();
    Serial.println("=== Profiles ===");
    int pCount = getProfileCount();
    int active = getActiveProfileIndex();
    if (pCount == 0) {
        Serial.println("  (none)");
    } else {
        for (int i = 0; i < pCount; i++) {
            Profile p;
            if (getProfile(i, p)) {
                String keyDisplay = p.apiKey.length() > 4
                    ? String("...") + p.apiKey.substring(p.apiKey.length() - 4)
                    : String("(none)");
                Serial.printf("  [%d]%s %s  model=%s  key=%s  tokens=%d\n",
                              i,
                              i == active ? "*" : " ",
                              p.name.c_str(),
                              p.model.c_str(),
                              keyDisplay.c_str(),
                              p.maxTokens);
            }
        }
        Serial.println("  (* = active)");
    }
    Serial.println();
}
