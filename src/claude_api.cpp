/*
 * claude_api.cpp - now reads config from the active profile
 */

#include "claude_api.h"
#include "storage.h"
#include "profile.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

ClaudeApi claudeApi;

String ClaudeApi::ask(const String& prompt) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[API] No WiFi");
        return "";
    }

    Profile p;
    if (!storage.getActiveProfile(p)) {
        Serial.println("[API] No active profile");
        return "";
    }
    if (p.apiKey.length() == 0) {
        Serial.println("[API] Active profile has no API key");
        return "";
    }

    JsonDocument req;
    req["model"]      = p.model;
    req["max_tokens"] = p.maxTokens;
    if (p.systemPrompt.length() > 0) {
        req["system"] = p.systemPrompt;
    }

    JsonArray messages = req["messages"].to<JsonArray>();
    JsonObject msg = messages.add<JsonObject>();
    msg["role"]    = "user";
    msg["content"] = prompt;

    String body;
    serializeJson(req, body);

    Serial.printf("[API] Profile '%s', model %s, %d bytes\n",
                  p.name.c_str(), p.model.c_str(), body.length());

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(20000);

    if (!http.begin(client, URL)) {
        Serial.println("[API] http.begin failed");
        return "";
    }

    http.addHeader("Content-Type",      "application/json");
    http.addHeader("x-api-key",         p.apiKey);
    http.addHeader("anthropic-version", API_VERSION);

    uint32_t t0 = millis();
    int status = http.POST(body);
    uint32_t elapsed = millis() - t0;

    Serial.printf("[API] HTTP %d in %lu ms\n", status, elapsed);

    if (status <= 0) {
        Serial.printf("[API] Failed: %s\n", http.errorToString(status).c_str());
        http.end();
        return "";
    }

    String respBody = http.getString();
    http.end();

    if (status != 200) {
        Serial.println("[API] Error response:");
        Serial.println(respBody);
        return "";
    }

    JsonDocument resp;
    DeserializationError err = deserializeJson(resp, respBody);
    if (err) {
        Serial.printf("[API] JSON parse error: %s\n", err.c_str());
        return "";
    }

    JsonArray content = resp["content"];
    if (content.isNull() || content.size() == 0) {
        Serial.println("[API] No content in response");
        return "";
    }

    String result;
    for (JsonObject block : content) {
        const char* type = block["type"];
        if (type && strcmp(type, "text") == 0) {
            const char* text = block["text"];
            if (text) result += text;
        }
    }

    JsonObject usage = resp["usage"];
    if (!usage.isNull()) {
        Serial.printf("[API] Tokens - in: %d, out: %d\n",
                      (int)usage["input_tokens"],
                      (int)usage["output_tokens"]);
    }

    return result;
}
