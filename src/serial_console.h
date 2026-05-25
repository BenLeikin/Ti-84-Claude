/*
 * serial_console.h
 */

#pragma once

#include <Arduino.h>

class SerialConsole {
public:
    void begin();
    void poll();

private:
    String _buffer;
    void handleLine(const String& line);

    void cmdHelp();
    void cmdList();
    void cmdStatus();
    void cmdReboot();
    void cmdAsk(const String& prompt);
    void cmdConnect();

    void cmdNetAdd(const String& args);
    void cmdNetDelete(const String& args);
    void cmdNetList();

    void cmdProfileAdd(const String& args);
    void cmdProfileDelete(const String& args);
    void cmdProfileUse(const String& args);
    void cmdProfileList();
    void cmdProfileEdit(const String& args);
};

extern SerialConsole console;
