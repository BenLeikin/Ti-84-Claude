/*
 * claude_api.h - Anthropic Messages API client
 *
 * Reads config from the active profile in storage on each call.
 */

#pragma once

#include <Arduino.h>

class ClaudeApi {
public:
    String ask(const String& prompt);

private:
    static constexpr const char* URL = "https://api.anthropic.com/v1/messages";
    static constexpr const char* API_VERSION = "2023-06-01";
};

extern ClaudeApi claudeApi;
