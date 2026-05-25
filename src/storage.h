/*
 * storage.h - NVS-backed multi-profile, multi-network configuration storage
 *
 * Storage layout:
 *
 *   "wifi_nets" namespace:
 *     count       - uint8, number of saved networks (0 to MAX_NETWORKS)
 *     net0..net4  - JSON blob per network slot
 *
 *   "profiles" namespace:
 *     count       - uint8, number of profiles (always >= 1)
 *     active      - uint8, index of currently active profile
 *     prof0..prof4 - JSON blob per profile slot
 *
 * Profiles maintain identity by index. Deleting a middle profile compacts
 * the array (shifts later ones down) so there are no gaps.
 */

#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "profile.h"
#include "wifi_network.h"

class Storage {
public:
    static constexpr int MAX_NETWORKS = 5;
    static constexpr int MAX_PROFILES = 5;

    bool begin();

    // -- WiFi networks --
    int  getNetworkCount();
    bool getNetwork(int index, WifiNetwork& out);
    bool addNetwork(const WifiNetwork& net);          // returns false if full
    bool updateNetwork(int index, const WifiNetwork& net);
    bool deleteNetwork(int index);
    bool moveNetworkUp(int index);                    // raise priority
    bool moveNetworkDown(int index);

    // -- Profiles --
    int  getProfileCount();
    int  getActiveProfileIndex();
    bool setActiveProfileIndex(int index);
    bool getProfile(int index, Profile& out);
    bool getActiveProfile(Profile& out);
    bool addProfile(const Profile& p);
    bool updateProfile(int index, const Profile& p);
    bool deleteProfile(int index);                    // refuses to delete last

    // -- Maintenance --
    void clearAll();
    void dumpToSerial();

    // First-boot setup: ensures at least one default profile exists
    void ensureBootstrap();

private:
    Preferences _wifiNets;
    Preferences _profiles;

    String wifiKey(int i)    { return String("net") + i; }
    String profileKey(int i) { return String("prof") + i; }
};

extern Storage storage;
