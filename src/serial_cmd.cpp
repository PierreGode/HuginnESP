#include "serial_cmd.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "scan_cycle.h"
#include "config.h"

volatile ScanMode g_currentMode   = MODE_AUTO_CYCLE;
volatile bool     g_manualOverride = false;

const char* scanModeName(ScanMode mode) {
    switch (mode) {
        case MODE_IDLE:         return "idle";
        case MODE_WIFI:         return "wifi";
        case MODE_BLE_FILTERED: return "ble_filtered";
        case MODE_BLE_ALL:      return "ble_all";
        case MODE_SKIMMER:      return "skimmer";
        case MODE_PINEAPPLE:    return "pineapple";
        case MODE_AUTO_CYCLE:   return "auto";
        default:                return "unknown";
    }
}

static void handleCommand(const String& cmd) {
    String c = cmd;
    c.trim();

    if (c == "scanap") {
        g_manualOverride = true;
        ble_scanner_stop();
        g_currentMode = MODE_WIFI;
        wifi_scanner_start();

    } else if (c == "blescan -f") {
        g_manualOverride = true;
        wifi_scanner_stop();
        g_currentMode = MODE_BLE_FILTERED;
        ble_scanner_start(BLE_MODE_FILTERED);

    } else if (c == "blescan -a") {
        g_manualOverride = true;
        wifi_scanner_stop();
        g_currentMode = MODE_BLE_ALL;
        ble_scanner_start(BLE_MODE_ALL);

    } else if (c == "capture -skimmer") {
        g_manualOverride = true;
        wifi_scanner_stop();
        g_currentMode = MODE_SKIMMER;
        ble_scanner_start(BLE_MODE_SKIMMER);

    } else if (c == "pineap") {
        g_manualOverride = true;
        ble_scanner_stop();
        g_currentMode = MODE_PINEAPPLE;
        wifi_scanner_check_pineapple();

    } else if (c == "stop" || c == "capture -stop") {
        g_manualOverride = false;
        wifi_scanner_stop();
        ble_scanner_stop();
        g_currentMode = MODE_AUTO_CYCLE;
        scan_cycle_resume();

    } else if (c == "status") {
        Serial.printf("{\"mode\":\"%s\",\"wifi_count\":%d,\"ble_count\":%d}\n",
                      scanModeName(g_currentMode),
                      wifi_scanner_count(),
                      ble_scanner_count());
    }
}

void serial_cmd_init() {
    // nothing extra needed — Serial is initialized in main
}

void serial_cmd_poll() {
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        if (line.length() > 0) {
            handleCommand(line);
        }
    }
}
