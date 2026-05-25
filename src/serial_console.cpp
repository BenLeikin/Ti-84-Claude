/*
 * serial_console.cpp - REPL for development and debugging
 */

#include "serial_console.h"
#include "storage.h"
#include "claude_api.h"
#include "profile.h"
#include "wifi_network.h"
#include "wifi_manager.h"
#include <WiFi.h>

SerialConsole console;

void SerialConsole::begin() {
    _buffer.reserve(256);
    Serial.println();
    Serial.println("Type HELP for available commands.");
    Serial.print("> ");
}

void SerialConsole::poll() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            String line = _buffer;
            line.trim();
            _buffer = "";
            if (line.length() > 0) handleLine(line);
            Serial.print("> ");
        } else {
            _buffer += c;
            if (_buffer.length() > 512) {
                _buffer = "";
                Serial.println("[Console] Input too long, cleared");
            }
        }
    }
}

// Split "CMD arg1 arg2..." into (cmd, rest)
static void splitCmd(const String& line, String& cmd, String& rest) {
    int sp = line.indexOf(' ');
    if (sp < 0) {
        cmd = line; rest = "";
    } else {
        cmd = line.substring(0, sp);
        rest = line.substring(sp + 1);
        rest.trim();
    }
    cmd.toUpperCase();
}

void SerialConsole::handleLine(const String& line) {
    String cmd, rest;
    splitCmd(line, cmd, rest);

    // Two-word commands like "NET ADD" or "PROFILE USE"
    String sub;
    if (cmd == "NET" || cmd == "PROFILE") {
        String r2;
        splitCmd(rest, sub, r2);
        rest = r2;
    }

    if (cmd == "HELP" || cmd == "?")       cmdHelp();
    else if (cmd == "LIST" || cmd == "LS") cmdList();
    else if (cmd == "STATUS")              cmdStatus();
    else if (cmd == "CONNECT")             cmdConnect();
    else if (cmd == "ASK")                 cmdAsk(rest);
    else if (cmd == "REBOOT")              cmdReboot();
    else if (cmd == "CLEAR") {
        if (rest == "confirm") storage.clearAll();
        else Serial.println("Type 'CLEAR confirm' to wipe all config");
    }
    else if (cmd == "NET") {
        if      (sub == "ADD")    cmdNetAdd(rest);
        else if (sub == "DELETE" || sub == "RM") cmdNetDelete(rest);
        else if (sub == "LIST"   || sub == "")   cmdNetList();
        else Serial.println("NET commands: ADD, DELETE, LIST");
    }
    else if (cmd == "PROFILE") {
        if      (sub == "ADD")    cmdProfileAdd(rest);
        else if (sub == "DELETE" || sub == "RM") cmdProfileDelete(rest);
        else if (sub == "USE")    cmdProfileUse(rest);
        else if (sub == "EDIT")   cmdProfileEdit(rest);
        else if (sub == "LIST"   || sub == "")   cmdProfileList();
        else Serial.println("PROFILE commands: ADD, DELETE, USE, EDIT, LIST");
    }
    else {
        Serial.printf("Unknown command: %s (try HELP)\n", cmd.c_str());
    }
}

void SerialConsole::cmdHelp() {
    Serial.println();
    Serial.println("General:");
    Serial.println("  LIST                       Show networks and profiles");
    Serial.println("  STATUS                     Show WiFi/system status");
    Serial.println("  CONNECT                    Connect using saved networks");
    Serial.println("  ASK <prompt>               Send a prompt to Claude");
    Serial.println("  REBOOT                     Restart the device");
    Serial.println("  CLEAR confirm              Wipe all configuration");
    Serial.println();
    Serial.println("Networks:");
    Serial.println("  NET LIST                   Show saved networks");
    Serial.println("  NET ADD <ssid> <password>  Add a network");
    Serial.println("  NET DELETE <index>         Remove a network by index");
    Serial.println();
    Serial.println("Profiles:");
    Serial.println("  PROFILE LIST               Show profiles");
    Serial.println("  PROFILE ADD <name>         Create a new profile");
    Serial.println("  PROFILE USE <index>        Set active profile");
    Serial.println("  PROFILE DELETE <index>     Remove a profile");
    Serial.println("  PROFILE EDIT <i> <field> <value>");
    Serial.println("                             fields: name, key, model, tokens, sys");
    Serial.println();
    Serial.println("For most editing, use the web UI at http://ti84claude.local");
    Serial.println();
}

void SerialConsole::cmdList()   { storage.dumpToSerial(); }
void SerialConsole::cmdReboot() {
    Serial.println("Rebooting...");
    Serial.flush(); delay(100); ESP.restart();
}

void SerialConsole::cmdConnect() {
    if (wifiManager.connectStation(20000)) {
        Serial.println("Connected.");
    } else {
        Serial.println("Connect failed. Check NET LIST and add a reachable network.");
    }
}

void SerialConsole::cmdStatus() {
    Serial.println();
    Serial.printf("WiFi status   : %s\n",
                  WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("SSID          : %s\n", WiFi.SSID().c_str());
        Serial.printf("IP            : %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("RSSI          : %d dBm\n", WiFi.RSSI());
    }
    Profile p;
    if (storage.getActiveProfile(p)) {
        Serial.printf("Active profile: %s (model %s)\n", p.name.c_str(), p.model.c_str());
    }
    Serial.printf("Networks      : %d\n", storage.getNetworkCount());
    Serial.printf("Profiles      : %d\n", storage.getProfileCount());
    Serial.printf("Free heap     : %d bytes\n", ESP.getFreeHeap());
    Serial.println();
}

void SerialConsole::cmdAsk(const String& prompt) {
    if (prompt.length() == 0) {
        Serial.println("Usage: ASK <prompt>");
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Not connected. Run CONNECT.");
        return;
    }

    String clean = prompt;
    if (clean.length() >= 2 &&
        ((clean.startsWith("\"") && clean.endsWith("\"")) ||
         (clean.startsWith("'")  && clean.endsWith("'")))) {
        clean = clean.substring(1, clean.length() - 1);
    }

    String response = claudeApi.ask(clean);
    Serial.println();
    Serial.println("----- CLAUDE -----");
    Serial.println(response.length() > 0 ? response : "(empty)");
    Serial.println("------------------");
}

// -- Network commands --

void SerialConsole::cmdNetList() { storage.dumpToSerial(); }

void SerialConsole::cmdNetAdd(const String& args) {
    int sp = args.indexOf(' ');
    if (sp < 0) {
        Serial.println("Usage: NET ADD <ssid> <password>");
        Serial.println("(use quoted strings for SSIDs with spaces in web UI)");
        return;
    }
    WifiNetwork net;
    net.ssid     = args.substring(0, sp);
    net.password = args.substring(sp + 1);
    if (storage.addNetwork(net)) {
        Serial.printf("Network '%s' added\n", net.ssid.c_str());
    } else {
        Serial.println("Failed (storage full?)");
    }
}

void SerialConsole::cmdNetDelete(const String& args) {
    if (args.length() == 0) { Serial.println("Usage: NET DELETE <index>"); return; }
    int i = args.toInt();
    if (storage.deleteNetwork(i)) {
        Serial.printf("Network [%d] deleted\n", i);
    } else {
        Serial.println("Invalid index");
    }
}

// -- Profile commands --

void SerialConsole::cmdProfileList() { storage.dumpToSerial(); }

void SerialConsole::cmdProfileAdd(const String& args) {
    if (args.length() == 0) { Serial.println("Usage: PROFILE ADD <name>"); return; }
    Profile p = Profile::makeDefault();
    p.name = args;
    if (storage.addProfile(p)) {
        Serial.printf("Profile '%s' created. Use PROFILE EDIT to set api key.\n",
                      p.name.c_str());
    } else {
        Serial.println("Failed (storage full?)");
    }
}

void SerialConsole::cmdProfileDelete(const String& args) {
    if (args.length() == 0) { Serial.println("Usage: PROFILE DELETE <index>"); return; }
    int i = args.toInt();
    if (storage.deleteProfile(i)) {
        Serial.printf("Profile [%d] deleted\n", i);
    } else {
        Serial.println("Cannot delete (invalid index, or it's the last profile)");
    }
}

void SerialConsole::cmdProfileUse(const String& args) {
    if (args.length() == 0) { Serial.println("Usage: PROFILE USE <index>"); return; }
    int i = args.toInt();
    if (storage.setActiveProfileIndex(i)) {
        Profile p;
        storage.getActiveProfile(p);
        Serial.printf("Active profile is now [%d] %s\n", i, p.name.c_str());
    } else {
        Serial.println("Invalid index");
    }
}

void SerialConsole::cmdProfileEdit(const String& args) {
    // PROFILE EDIT <i> <field> <value>
    int sp1 = args.indexOf(' ');
    if (sp1 < 0) {
        Serial.println("Usage: PROFILE EDIT <index> <field> <value>");
        Serial.println("fields: name, key, model, tokens, sys");
        return;
    }
    int sp2 = args.indexOf(' ', sp1 + 1);
    if (sp2 < 0) {
        Serial.println("Usage: PROFILE EDIT <index> <field> <value>");
        return;
    }
    int i        = args.substring(0, sp1).toInt();
    String field = args.substring(sp1 + 1, sp2);
    String value = args.substring(sp2 + 1);
    field.toLowerCase();

    Profile p;
    if (!storage.getProfile(i, p)) { Serial.println("Invalid index"); return; }

    if      (field == "name")   p.name = value;
    else if (field == "key")    p.apiKey = value;
    else if (field == "model") {
        if (value.indexOf('.') >= 0) {
            Serial.println("Model strings use dashes, e.g. claude-haiku-4-5");
            return;
        }
        p.model = value;
    }
    else if (field == "tokens") {
        int t = value.toInt();
        if (t < 1 || t > 4096) { Serial.println("tokens out of range"); return; }
        p.maxTokens = t;
    }
    else if (field == "sys")    p.systemPrompt = value;
    else { Serial.printf("Unknown field: %s\n", field.c_str()); return; }

    storage.updateProfile(i, p);
    Serial.printf("Profile [%d] %s updated\n", i, field.c_str());
}
