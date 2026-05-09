#ifndef CONFIG_H
#define CONFIG_H

// ----- Serial -----
#define SERIAL_BAUD 115200

// ----- Firmware version -----
// Bumped manually; emitted in the device announce line at boot. Should
// match the version reported by the web flasher's manifest.json.
#define HUGINN_FW_VERSION "1.0"

// ----- Scan durations (ms) -----
#define WIFI_SCAN_DURATION   15000
#define BLE_SCAN_DURATION     8000
#define PINEAP_SCAN_DURATION 10000

// ----- BLE parameters -----
#define BLE_SCAN_WINDOW_MS   8000
#define BLE_SPAM_THRESHOLD     20   // advertisements from one MAC within window
#define BLE_SPAM_WINDOW_MS   5000

// ----- Display -----
#define SCREEN_WIDTH   480
#define SCREEN_HEIGHT  480

// ----- Task stack sizes -----
#define WIFI_TASK_STACK   8192
#define BLE_TASK_STACK    8192
#define DISPLAY_TASK_STACK 8192
#define SERIAL_TASK_STACK  4096
#define CYCLE_TASK_STACK   8192

// ----- Max tracked items -----
#define MAX_DISPLAY_DEVICES 10
#define MAX_WIFI_NETWORKS  100
#define MAX_BLE_DEVICES    100

// ----- Skimmer suspicious names (defaults; runtime list lives in runtime_config) -----
static const char* SKIMMER_NAMES_DEFAULT[] = {
    "HC-05", "HC-06", "HC-08",
    "BT05", "BT06",
    "JDY-30", "JDY-31", "JDY-33",
    "SPP-CA",
    nullptr
};

// ----- Flipper Zero BLE identification -----
// Flipper Zero manufacturer data company ID (0x4C01 is the placeholder;
// real identification uses service UUID + manufacturer data heuristics)
#define FLIPPER_SERVICE_UUID "8e400001-f315-4f60-9fb8-838830daea50"

// ----- AirTag identification -----
// Apple company ID in BLE manufacturer data
#define APPLE_COMPANY_ID 0x004C

#endif // CONFIG_H
