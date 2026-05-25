/*
 * captive_portal.cpp - tabbed multi-page web UI
 *
 * Routes:
 *   /                 → networks list
 *   /networks         → networks list (same as /)
 *   /networks/new     → form for new network
 *   /networks/edit?i= → form for editing network at index i
 *   /networks/save    → POST handler
 *   /networks/delete?i=
 *   /networks/up?i=, /networks/down?i=
 *
 *   /profiles         → profiles list
 *   /profiles/new     → form for new profile
 *   /profiles/edit?i= → edit profile at index i
 *   /profiles/save    → POST handler
 *   /profiles/delete?i=
 *   /profiles/use?i=  → set active profile
 *
 *   /status           → connection info
 *   /reboot
 *   /reset            → factory reset
 */

#include "captive_portal.h"
#include "storage.h"
#include "wifi_manager.h"
#include "profile.h"
#include "wifi_network.h"
#include <WiFi.h>

CaptivePortal portal;

// ---------------------------------------------------------------------------
// Stylesheet
// ---------------------------------------------------------------------------

static const char* CSS = R"CSS(
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  background: #0f1419;
  color: #e4e6eb;
  padding: 1rem;
  min-height: 100vh;
}
.container { max-width: 520px; margin: 0 auto; }
h1 { font-size: 1.4rem; color: #fff; margin-bottom: 0.25rem; }
h2 { font-size: 1.1rem; color: #fff; margin-bottom: 0.75rem; }
.subtitle { color: #8b949e; font-size: 0.85rem; margin-bottom: 1.25rem; }
.tabs { display: flex; gap: 0.25rem; margin-bottom: 1rem;
        border-bottom: 1px solid #2d3548; }
.tabs a {
  padding: 0.6rem 1rem;
  color: #8b949e;
  text-decoration: none;
  border-bottom: 2px solid transparent;
  font-size: 0.95rem;
}
.tabs a.active { color: #fff; border-bottom-color: #4a8eff; }
.card {
  background: #1a1f2e;
  border: 1px solid #2d3548;
  border-radius: 8px;
  padding: 1rem;
  margin-bottom: 0.75rem;
}
.list-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0.65rem 0;
  border-bottom: 1px solid #2d3548;
}
.list-item:last-child { border-bottom: none; }
.list-item .info { flex: 1; min-width: 0; }
.list-item .name { color: #fff; font-weight: 500; }
.list-item .meta { color: #8b949e; font-size: 0.8rem; }
.list-item .actions { display: flex; gap: 0.35rem; flex-shrink: 0; }
label {
  display: block;
  font-size: 0.85rem;
  color: #8b949e;
  margin-bottom: 0.35rem;
  margin-top: 0.75rem;
}
label:first-child { margin-top: 0; }
input[type=text], input[type=password], input[type=number], select, textarea {
  width: 100%;
  padding: 0.55rem 0.7rem;
  background: #0f1419;
  border: 1px solid #2d3548;
  border-radius: 4px;
  color: #e4e6eb;
  font-size: 0.95rem;
  font-family: inherit;
}
textarea { resize: vertical; min-height: 70px; }
input:focus, select:focus, textarea:focus {
  outline: none; border-color: #4a8eff;
}
.btn, button {
  display: inline-block;
  background: #4a8eff;
  color: white;
  border: none;
  padding: 0.45rem 0.85rem;
  border-radius: 4px;
  font-size: 0.85rem;
  font-weight: 500;
  cursor: pointer;
  text-decoration: none;
  text-align: center;
}
.btn:hover, button:hover { background: #3a7eef; }
.btn-primary { padding: 0.6rem 1.1rem; font-size: 0.95rem; margin-top: 1rem; }
.btn-secondary { background: #2d3548; }
.btn-secondary:hover { background: #3d4558; }
.btn-danger { background: #cc3333; }
.btn-danger:hover { background: #aa2222; }
.btn-icon { padding: 0.4rem 0.55rem; font-weight: bold; }
.empty { color: #6b7280; padding: 1rem 0; text-align: center; font-style: italic; }
.hint { font-size: 0.78rem; color: #6b7280; margin-top: 0.3rem; }
.status-row {
  display: flex; justify-content: space-between;
  padding: 0.4rem 0;
  border-bottom: 1px solid #2d3548;
  font-size: 0.9rem;
}
.status-row:last-child { border-bottom: none; }
.status-row .label { color: #8b949e; }
.status-row .value { color: #e4e6eb; font-family: monospace; }
.success { background: #1f3a2e; border-color: #2d5a44; color: #84d99f; padding: 1rem; }
.badge {
  display: inline-block;
  background: #2d5a44;
  color: #84d99f;
  padding: 0.15rem 0.5rem;
  border-radius: 3px;
  font-size: 0.75rem;
  margin-left: 0.5rem;
}
)CSS";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static String htmlEscape(const String& s) {
    String out;
    out.reserve(s.length() + 16);
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s.charAt(i);
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;
        }
    }
    return out;
}

String CaptivePortal::renderPage(const String& title, const String& tab, const String& body) {
    String html;
    html.reserve(8192);
    html += "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>"; html += title; html += "</title>";
    html += "<style>"; html += CSS; html += "</style></head><body><div class=\"container\">";
    html += "<h1>TI-84 Claude Bridge</h1>";

    // Tab bar
    html += "<div class=\"tabs\">";
    auto tabLink = [&](const char* href, const char* label, bool active) {
        html += "<a href=\""; html += href; html += "\"";
        if (active) html += " class=\"active\"";
        html += ">"; html += label; html += "</a>";
    };
    tabLink("/networks", "WiFi",     tab == "networks");
    tabLink("/profiles", "Profiles", tab == "profiles");
    tabLink("/status",   "Status",   tab == "status");
    html += "</div>";

    html += body;
    html += "</div></body></html>";
    return html;
}

// ---------------------------------------------------------------------------
// Networks
// ---------------------------------------------------------------------------

String CaptivePortal::renderNetworksList() {
    String body;
    body += "<div class=\"card\">";
    body += "<h2>Saved Networks</h2>";

    int count = storage.getNetworkCount();
    if (count == 0) {
        body += "<div class=\"empty\">No networks saved yet</div>";
    } else {
        body += "<p class=\"hint\" style=\"margin-bottom:0.5rem\">";
        body += "Listed in priority order. Device connects to the highest-priority ";
        body += "network that's both visible and reachable.</p>";

        for (int i = 0; i < count; i++) {
            WifiNetwork net;
            if (!storage.getNetwork(i, net)) continue;
            body += "<div class=\"list-item\">";
            body += "<div class=\"info\">";
            body += "<div class=\"name\">"; body += htmlEscape(net.ssid); body += "</div>";
            body += "<div class=\"meta\">Priority "; body += String(i + 1); body += "</div>";
            body += "</div><div class=\"actions\">";
            if (i > 0) {
                body += "<a class=\"btn btn-icon btn-secondary\" href=\"/networks/up?i="
                     + String(i) + "\" title=\"Move up\">&uarr;</a>";
            }
            if (i < count - 1) {
                body += "<a class=\"btn btn-icon btn-secondary\" href=\"/networks/down?i="
                     + String(i) + "\" title=\"Move down\">&darr;</a>";
            }
            body += "<a class=\"btn btn-secondary\" href=\"/networks/edit?i="
                 + String(i) + "\">Edit</a>";
            body += "<a class=\"btn btn-danger\" href=\"/networks/delete?i=" + String(i)
                 + "\" onclick=\"return confirm('Delete this network?')\">&times;</a>";
            body += "</div></div>";
        }
    }

    if (count < Storage::MAX_NETWORKS) {
        body += "<a class=\"btn btn-primary\" href=\"/networks/new\">+ Add network</a>";
    } else {
        body += "<p class=\"hint\" style=\"margin-top:0.75rem\">";
        body += "Maximum of "; body += String(Storage::MAX_NETWORKS); body += " networks reached.</p>";
    }
    body += "</div>";

    return renderPage("WiFi Networks", "networks", body);
}

String CaptivePortal::renderNetworkForm(int index) {
    bool isNew = (index < 0);
    WifiNetwork net;
    if (!isNew) storage.getNetwork(index, net);

    String body;
    body += "<div class=\"card\"><h2>";
    body += isNew ? "Add Network" : "Edit Network";
    body += "</h2>";

    body += "<form method=\"POST\" action=\"/networks/save\">";
    if (!isNew) {
        body += "<input type=\"hidden\" name=\"i\" value=\"" + String(index) + "\">";
    }
    body += "<label>Network name (SSID)</label>";
    body += "<input type=\"text\" name=\"ssid\" value=\""
         + htmlEscape(net.ssid) + "\" required>";
    body += "<label>Password</label>";
    body += "<input type=\"password\" name=\"pass\" value=\""
         + htmlEscape(net.password) + "\">";
    body += "<p class=\"hint\">Leave blank for open networks. 2.4 GHz only.</p>";

    body += "<button type=\"submit\" class=\"btn-primary\">Save</button> ";
    body += "<a class=\"btn btn-secondary\" href=\"/networks\">Cancel</a>";
    body += "</form></div>";

    return renderPage(isNew ? "Add Network" : "Edit Network", "networks", body);
}

// ---------------------------------------------------------------------------
// Profiles
// ---------------------------------------------------------------------------

String CaptivePortal::renderProfilesList() {
    String body;
    body += "<div class=\"card\">";
    body += "<h2>Claude Profiles</h2>";

    int count = storage.getProfileCount();
    int active = storage.getActiveProfileIndex();

    for (int i = 0; i < count; i++) {
        Profile p;
        if (!storage.getProfile(i, p)) continue;

        body += "<div class=\"list-item\">";
        body += "<div class=\"info\">";
        body += "<div class=\"name\">"; body += htmlEscape(p.name);
        if (i == active) body += "<span class=\"badge\">active</span>";
        body += "</div>";
        body += "<div class=\"meta\">"; body += htmlEscape(p.model);
        body += " | "; body += String(p.maxTokens); body += " tokens";
        body += " | key "; body += (p.apiKey.length() > 0 ? "set" : "not set");
        body += "</div>";
        body += "</div><div class=\"actions\">";
        if (i != active) {
            body += "<a class=\"btn\" href=\"/profiles/use?i=" + String(i)
                 + "\">Use</a>";
        }
        body += "<a class=\"btn btn-secondary\" href=\"/profiles/edit?i="
             + String(i) + "\">Edit</a>";
        if (count > 1) {
            body += "<a class=\"btn btn-danger\" href=\"/profiles/delete?i=" + String(i)
                 + "\" onclick=\"return confirm('Delete this profile?')\">&times;</a>";
        }
        body += "</div></div>";
    }

    if (count < Storage::MAX_PROFILES) {
        body += "<a class=\"btn btn-primary\" href=\"/profiles/new\">+ Add profile</a>";
    }
    body += "</div>";

    return renderPage("Profiles", "profiles", body);
}

String CaptivePortal::renderProfileForm(int index) {
    bool isNew = (index < 0);
    Profile p;
    if (isNew) {
        p = Profile::makeDefault();
        p.name = "";  // user must name it
    } else {
        if (!storage.getProfile(index, p)) {
            return renderPage("Error", "profiles",
                "<div class=\"card\">Profile not found</div>");
        }
    }

    String body;
    body += "<div class=\"card\"><h2>";
    body += isNew ? "Add Profile" : "Edit Profile";
    body += "</h2>";

    body += "<form method=\"POST\" action=\"/profiles/save\">";
    if (!isNew) {
        body += "<input type=\"hidden\" name=\"i\" value=\"" + String(index) + "\">";
    }

    body += "<label>Name</label>";
    body += "<input type=\"text\" name=\"name\" value=\""
         + htmlEscape(p.name) + "\" required maxlength=\"24\" "
         + "placeholder=\"e.g. work, personal, kid-mode\">";

    body += "<label>API key</label>";
    body += "<input type=\"password\" name=\"api_key\" value=\""
         + htmlEscape(p.apiKey) + "\" placeholder=\"sk-ant-api03-...\" required>";
    body += "<p class=\"hint\">Get a key at console.anthropic.com</p>";

    body += "<label>Model</label>";
    body += "<select name=\"model\">";
    const char* models[] = {
        "claude-haiku-4-5",
        "claude-sonnet-4-6",
        "claude-opus-4-7"
    };
    const char* labels[] = {
        "Claude Haiku 4.5 (fastest, cheapest)",
        "Claude Sonnet 4.6 (balanced)",
        "Claude Opus 4.7 (most capable, expensive)"
    };
    for (int i = 0; i < 3; i++) {
        body += "<option value=\""; body += models[i]; body += "\"";
        if (p.model == models[i]) body += " selected";
        body += ">"; body += labels[i]; body += "</option>";
    }
    body += "</select>";

    body += "<label>Max response tokens</label>";
    body += "<input type=\"number\" name=\"max_tokens\" value=\""
         + String(p.maxTokens) + "\" min=\"1\" max=\"4096\">";

    body += "<label>System prompt</label>";
    body += "<textarea name=\"sys_prompt\" rows=\"4\">"
         + htmlEscape(p.systemPrompt) + "</textarea>";

    body += "<button type=\"submit\" class=\"btn-primary\">Save</button> ";
    body += "<a class=\"btn btn-secondary\" href=\"/profiles\">Cancel</a>";
    body += "</form></div>";

    return renderPage(isNew ? "Add Profile" : "Edit Profile", "profiles", body);
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

String CaptivePortal::renderStatusPage() {
    String body;
    body += "<div class=\"card\">";

    auto row = [&](const String& label, const String& value) {
        body += "<div class=\"status-row\"><span class=\"label\">";
        body += label; body += "</span><span class=\"value\">";
        body += value; body += "</span></div>";
    };

    if (wifiManager.currentMode() == WifiMode::STATION) {
        row("WiFi mode",  "Station");
        row("SSID",       WiFi.SSID());
        row("IP address", WiFi.localIP().toString());
        row("RSSI",       String(WiFi.RSSI()) + " dBm");
    } else {
        row("WiFi mode",  "Access Point");
        row("AP SSID",    String(WifiManager::AP_SSID));
        row("AP IP",      WiFi.softAPIP().toString());
        row("Clients",    String(WiFi.softAPgetStationNum()));
    }

    Profile p;
    if (storage.getActiveProfile(p)) {
        row("Active profile", p.name);
        row("Model",          p.model);
    } else {
        row("Active profile", "(none)");
    }
    row("Saved networks", String(storage.getNetworkCount()));
    row("Profiles",       String(storage.getProfileCount()));
    row("MAC",            WiFi.macAddress());
    row("Free heap",      String(ESP.getFreeHeap()) + " bytes");
    row("Uptime",         String(millis() / 1000) + " s");
    body += "</div>";

    body += "<div class=\"card\">";
    body += "<a class=\"btn btn-secondary\" href=\"/reboot\" "
            "onclick=\"return confirm('Reboot device?')\">Reboot</a> ";
    body += "<a class=\"btn btn-danger\" href=\"/reset\" "
            "onclick=\"return confirm('Wipe ALL config? This cannot be undone.')\">Factory reset</a>";
    body += "</div>";

    return renderPage("Status", "status", body);
}

String CaptivePortal::renderSavedPage() {
    String body;
    body += "<div class=\"card success\">";
    body += "<h2>Saved</h2>";
    body += "<p>Configuration updated.</p>";
    body += "<p style=\"margin-top:0.5rem\"><a class=\"btn\" href=\"/\">Back</a></p>";
    body += "</div>";
    return renderPage("Saved", "", body);
}

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------

void CaptivePortal::handleNetworksList() {
    _server.send(200, "text/html", renderNetworksList());
}

void CaptivePortal::handleNetworkNew() {
    _server.send(200, "text/html", renderNetworkForm(-1));
}

void CaptivePortal::handleNetworkEdit() {
    int i = _server.hasArg("i") ? _server.arg("i").toInt() : -1;
    _server.send(200, "text/html", renderNetworkForm(i));
}

void CaptivePortal::handleNetworkSave() {
    WifiNetwork net;
    net.ssid     = _server.arg("ssid");
    net.password = _server.arg("pass");

    if (_server.hasArg("i") && _server.arg("i").length() > 0) {
        storage.updateNetwork(_server.arg("i").toInt(), net);
    } else {
        if (!storage.addNetwork(net)) {
            _server.send(200, "text/html",
                renderPage("Error", "networks",
                    "<div class=\"card\">Maximum networks reached</div>"));
            return;
        }
    }
    _server.sendHeader("Location", "/networks");
    _server.send(303);
}

void CaptivePortal::handleNetworkDelete() {
    if (_server.hasArg("i")) storage.deleteNetwork(_server.arg("i").toInt());
    _server.sendHeader("Location", "/networks");
    _server.send(303);
}

void CaptivePortal::handleNetworkUp() {
    if (_server.hasArg("i")) storage.moveNetworkUp(_server.arg("i").toInt());
    _server.sendHeader("Location", "/networks");
    _server.send(303);
}

void CaptivePortal::handleNetworkDown() {
    if (_server.hasArg("i")) storage.moveNetworkDown(_server.arg("i").toInt());
    _server.sendHeader("Location", "/networks");
    _server.send(303);
}

void CaptivePortal::handleProfilesList() {
    _server.send(200, "text/html", renderProfilesList());
}

void CaptivePortal::handleProfileNew() {
    _server.send(200, "text/html", renderProfileForm(-1));
}

void CaptivePortal::handleProfileEdit() {
    int i = _server.hasArg("i") ? _server.arg("i").toInt() : -1;
    _server.send(200, "text/html", renderProfileForm(i));
}

void CaptivePortal::handleProfileSave() {
    Profile p;
    p.name         = _server.arg("name");
    p.apiKey       = _server.arg("api_key");
    p.model        = _server.arg("model");
    p.maxTokens    = _server.arg("max_tokens").toInt();
    if (p.maxTokens < 1)    p.maxTokens = 256;
    if (p.maxTokens > 4096) p.maxTokens = 4096;
    p.systemPrompt = _server.arg("sys_prompt");

    if (_server.hasArg("i") && _server.arg("i").length() > 0) {
        storage.updateProfile(_server.arg("i").toInt(), p);
    } else {
        if (!storage.addProfile(p)) {
            _server.send(200, "text/html",
                renderPage("Error", "profiles",
                    "<div class=\"card\">Maximum profiles reached</div>"));
            return;
        }
    }
    _server.sendHeader("Location", "/profiles");
    _server.send(303);
}

void CaptivePortal::handleProfileDelete() {
    if (_server.hasArg("i")) storage.deleteProfile(_server.arg("i").toInt());
    _server.sendHeader("Location", "/profiles");
    _server.send(303);
}

void CaptivePortal::handleProfileUse() {
    if (_server.hasArg("i")) storage.setActiveProfileIndex(_server.arg("i").toInt());
    _server.sendHeader("Location", "/profiles");
    _server.send(303);
}

void CaptivePortal::handleStatus() {
    _server.send(200, "text/html", renderStatusPage());
}

void CaptivePortal::handleReboot() {
    _server.send(200, "text/html",
        renderPage("Rebooting", "", "<div class=\"card\"><h2>Rebooting...</h2></div>"));
    _server.client().flush();
    delay(800);
    ESP.restart();
}

void CaptivePortal::handleFactoryReset() {
    storage.clearAll();
    _server.send(200, "text/html",
        renderPage("Reset", "", "<div class=\"card success\"><h2>Wiped</h2>"
                                "<p>Device is rebooting into setup mode.</p></div>"));
    _server.client().flush();
    delay(1200);
    ESP.restart();
}

void CaptivePortal::handleNotFound() {
    // Captive portal: any unknown URL serves the networks page so phones
    // detect "internet" and fire the setup popup.
    _server.send(200, "text/html", renderNetworksList());
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void CaptivePortal::installRoutes() {
    _server.on("/",                  HTTP_GET,  [this]{ handleNetworksList(); });
    _server.on("/networks",          HTTP_GET,  [this]{ handleNetworksList(); });
    _server.on("/networks/new",      HTTP_GET,  [this]{ handleNetworkNew(); });
    _server.on("/networks/edit",     HTTP_GET,  [this]{ handleNetworkEdit(); });
    _server.on("/networks/save",     HTTP_POST, [this]{ handleNetworkSave(); });
    _server.on("/networks/delete",   HTTP_GET,  [this]{ handleNetworkDelete(); });
    _server.on("/networks/up",       HTTP_GET,  [this]{ handleNetworkUp(); });
    _server.on("/networks/down",     HTTP_GET,  [this]{ handleNetworkDown(); });

    _server.on("/profiles",          HTTP_GET,  [this]{ handleProfilesList(); });
    _server.on("/profiles/new",      HTTP_GET,  [this]{ handleProfileNew(); });
    _server.on("/profiles/edit",     HTTP_GET,  [this]{ handleProfileEdit(); });
    _server.on("/profiles/save",     HTTP_POST, [this]{ handleProfileSave(); });
    _server.on("/profiles/delete",   HTTP_GET,  [this]{ handleProfileDelete(); });
    _server.on("/profiles/use",      HTTP_GET,  [this]{ handleProfileUse(); });

    _server.on("/status",            HTTP_GET,  [this]{ handleStatus(); });
    _server.on("/reboot",            HTTP_GET,  [this]{ handleReboot(); });
    _server.on("/reset",             HTTP_GET,  [this]{ handleFactoryReset(); });

    // Captive portal probe URLs
    _server.on("/generate_204",        HTTP_GET, [this]{ handleNotFound(); });
    _server.on("/hotspot-detect.html", HTTP_GET, [this]{ handleNotFound(); });
    _server.on("/connecttest.txt",     HTTP_GET, [this]{ handleNotFound(); });
    _server.on("/ncsi.txt",            HTTP_GET, [this]{ handleNotFound(); });
    _server.on("/fwlink",              HTTP_GET, [this]{ handleNotFound(); });
}

void CaptivePortal::beginAP() {
    _dns.setErrorReplyCode(DNSReplyCode::NoError);
    _dns.start(53, "*", WiFi.softAPIP());
    _dnsActive = true;

    installRoutes();
    _server.onNotFound([this]{ handleNotFound(); });
    _server.begin();
    _running = true;
    Serial.println("[Portal] Captive portal running on http://192.168.4.1");
}

void CaptivePortal::beginSTA() {
    installRoutes();
    _server.onNotFound([this]{ _server.send(404, "text/plain", "Not found"); });
    _server.begin();
    _running = true;
    Serial.printf("[Portal] Web UI on http://%s and http://ti84claude.local\n",
                  WiFi.localIP().toString().c_str());
}

void CaptivePortal::stop() {
    if (_dnsActive) { _dns.stop(); _dnsActive = false; }
    _server.stop();
    _running = false;
}

void CaptivePortal::poll() {
    if (!_running) return;
    if (_dnsActive) _dns.processNextRequest();
    _server.handleClient();
}
