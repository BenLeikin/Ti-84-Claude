/*
 * profile.cpp
 */

#include "profile.h"

String Profile::toJson() const {
    JsonDocument doc;
    doc["name"]    = name;
    doc["api_key"] = apiKey;
    doc["model"]   = model;
    doc["tokens"]  = maxTokens;
    doc["sys"]     = systemPrompt;

    String out;
    serializeJson(doc, out);
    return out;
}

bool Profile::fromJson(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    name         = doc["name"]    | "";
    apiKey       = doc["api_key"] | "";
    model        = doc["model"]   | "claude-haiku-4-5";
    maxTokens    = doc["tokens"]  | 256;
    systemPrompt = doc["sys"]     | "";

    return name.length() > 0;
}

Profile Profile::makeDefault() {
    Profile p;
    p.name         = "default";
    p.apiKey       = "";
    p.model        = "claude-haiku-4-5";
    p.maxTokens    = 256;
    p.systemPrompt =
        "You are an assistant being accessed from a TI-84 calculator with a "
        "96x64 monochrome display. Keep responses extremely concise, under "
        "60 words. Plain text only. No markdown, no tables, no emoji.";
    return p;
}
