#ifndef SERIAL_CMD_H
#define SERIAL_CMD_H

#include <Arduino.h>

// Current scan mode (shared state for display & cycle)
enum ScanMode {
    MODE_IDLE,
    MODE_WIFI,
    MODE_BLE_FILTERED,
    MODE_BLE_ALL,
    MODE_SKIMMER,
    MODE_PINEAPPLE,
    MODE_AUTO_CYCLE,
    MODE_WARDRIVE
};

extern volatile ScanMode g_currentMode;
extern volatile bool     g_manualOverride;

void serial_cmd_init();
void serial_cmd_poll();   // call from loop or task

// Return human-readable name for current mode
const char* scanModeName(ScanMode mode);

#endif // SERIAL_CMD_H
