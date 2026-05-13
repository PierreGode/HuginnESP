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
        Serial.println("[HUGINN] OK mode=wifi");

    } else if (c == "blescan -f") {
        g_manualOverride = true;
        wifi_scanner_stop();
        g_currentMode = MODE_BLE_FILTERED;
        ble_scanner_start(BLE_MODE_FILTERED);
        Serial.println("[HUGINN] OK mode=ble_filtered");

    } else if (c == "blescan -a") {
        g_manualOverride = true;
        wifi_scanner_stop();
        g_currentMode = MODE_BLE_ALL;
        ble_scanner_start(BLE_MODE_ALL);
        Serial.println("[HUGINN] OK mode=ble_all");

    } else if (c == "capture -skimmer") {
        g_manualOverride = true;
        wifi_scanner_stop();
        g_currentMode = MODE_SKIMMER;
        ble_scanner_start(BLE_MODE_SKIMMER);
        Serial.println("[HUGINN] OK mode=skimmer");

    } else if (c == "pineap") {
        g_manualOverride = true;
        ble_scanner_stop();
        g_currentMode = MODE_PINEAPPLE;
        wifi_scanner_check_pineapple();
        Serial.println("[HUGINN] OK mode=pineapple");

    } else if (c == "wardrive") {
        g_manualOverride = true;
        wifi_scanner_stop();
        ble_scanner_stop();
        g_currentMode = MODE_WARDRIVE;
        Serial.println("[HUGINN] OK mode=wardrive");

    } else if (c == "stop" || c == "capture -stop") {
        g_manualOverride = false;
        wifi_scanner_stop();
        ble_scanner_stop();
        g_currentMode = MODE_AUTO_CYCLE;
        scan_cycle_resume();
        Serial.println("[HUGINN] OK mode=auto");

    } else if (c == "status") {
        Serial.printf("[HUGINN] {\"mode\":\"%s\",\"wifi_count\":%d,\"ble_count\":%d}\n",
                      scanModeName(g_currentMode),
                      wifi_scanner_count(),
                      ble_scanner_count());

    } else if (c == "usbnet on") {
        usb_net_enable();
        Serial.print("[HUGINN] ");
        Serial.println(usb_net_status_json());

    } else if (c == "usbnet off") {
        usb_net_disable();
        Serial.print("[HUGINN] ");
        Serial.println(usb_net_status_json());

    } else if (c == "usbnet" || c == "usbnet status") {
        Serial.print("[HUGINN] ");
        Serial.println(usb_net_status_json());

    } else if (c == "help") {
        Serial.println("[HUGINN] Commands:");
        Serial.println("[HUGINN]   scanap            Wi-Fi scan");
        Serial.println("[HUGINN]   blescan -f        BLE filtered scan");
        Serial.println("[HUGINN]   blescan -a        BLE all scan");
        Serial.println("[HUGINN]   capture -skimmer  Skimmer detection");
        Serial.println("[HUGINN]   pineap            Pineapple detection");
        Serial.println("[HUGINN]   wardrive          Wardrive mode");
        Serial.println("[HUGINN]   stop              Auto cycle (default)");
        Serial.println("[HUGINN]   status            JSON status");
        Serial.println("[HUGINN]   usbnet on|off     USB network");
        Serial.println("[HUGINN]   usbnet status     USB network status");
        Serial.println("[HUGINN]   set <key> <val>   Set config value");
        Serial.println("[HUGINN]   get <key|all>     Get config value(s)");
        Serial.println("[HUGINN]   help              This help");

    } else if (c.length() > 0) {
        Serial.printf("[HUGINN] ERR unknown command: %s\n", c.c_str());
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
