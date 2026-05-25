/*
 * profile.h - Claude profile data structure
 *
 * A profile bundles all the settings that affect how Claude responds:
 * which model, what system prompt, how verbose, and which API key.
 *
 * Users can have multiple profiles and switch between them. This lets one
 * device serve different use cases (work vs personal, kid-friendly mode, etc.)
 * without reconfiguring every time.
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

struct Profile {
    String name;
    String apiKey;
    String model;
    int    maxTokens;
    String systemPrompt;

    // JSON serialization for NVS storage
    String toJson() const;
    bool fromJson(const String& json);

    // Default profile factory
    static Profile makeDefault();
};
