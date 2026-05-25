/*
 * captive_portal.h - tabbed web UI for multi-network and multi-profile config
 */

#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>

class CaptivePortal {
public:
    void beginAP();
    void beginSTA();
    void stop();
    void poll();
    bool isRunning() const { return _running; }

private:
    WebServer _server{80};
    DNSServer _dns;
    bool _running = false;
    bool _dnsActive = false;

    void installRoutes();

    // Page renderers
    String renderPage(const String& title, const String& tab, const String& body);
    String renderNetworksList();
    String renderNetworkForm(int index);     // -1 for new
    String renderProfilesList();
    String renderProfileForm(int index);     // -1 for new
    String renderStatusPage();
    String renderSavedPage();

    // Handlers
    void handleNetworksList();
    void handleNetworkNew();
    void handleNetworkEdit();
    void handleNetworkSave();
    void handleNetworkDelete();
    void handleNetworkUp();
    void handleNetworkDown();

    void handleProfilesList();
    void handleProfileNew();
    void handleProfileEdit();
    void handleProfileSave();
    void handleProfileDelete();
    void handleProfileUse();

    void handleStatus();
    void handleReboot();
    void handleFactoryReset();
    void handleNotFound();
};

extern CaptivePortal portal;
