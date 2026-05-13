#include "serial_cmd.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "scan_cycle.h"
#include "config.h"
#include "runtime_config.h"
#include "usb_net.h"

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
        case MODE_WARDRIVE:     return "wardrive";
        default:                return "unknown";
    }
}

static void handleCommand(const String& cmd) {
    String c = cmd;
    c.trim();

    // Runtime config (`set <key> <value>` / `get <key>`) — additive, doesn't
    // touch existing verbs. If the line wasn't `set ...` or `get ...`, this
    // returns false and we fall through to the original command table.
    if (runtime_config_handle(c)) return;

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

    } else if (c == "wardrive") {
        // Tight WiFi/BLE alternation tuned for moving captures. Loop runs
        // in scan_cycle_task while g_currentMode == MODE_WARDRIVE.
        g_manualOverride = true;
        wifi_scanner_stop();
        ble_scanner_stop();
        g_currentMode = MODE_WARDRIVE;

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

    } else if (c == "usbnet on") {
        usb_net_enable();
        Serial.println(usb_net_status_json());

    } else if (c == "usbnet off") {
        usb_net_disable();
        Serial.println(usb_net_status_json());

    } else if (c == "usbnet" || c == "usbnet status") {
        Serial.println(usb_net_status_json());
    }
}

void serial_cmd_init() {
    // nothing extra needed — Serial is initialized in main
}

// C-ABI shim so the HTTP server (web_portal.cpp) can feed command lines
// through the same parser the serial port uses. Used by POST /api/cmd.
extern "C" void huginn_dispatch_cmd_line(const char* line) {
    if (!line || !*line) return;
    handleCommand(String(line));
}

void serial_cmd_poll() {
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        if (line.length() > 0) {
            handleCommand(line);
        }
    }
}
