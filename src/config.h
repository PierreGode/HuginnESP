#ifndef CONFIG_H
#define CONFIG_H

// ----- Serial -----
// 460800 8N1: gives ~4× headroom over the old 115200 cap so the BLE callback
// doesn't stall in Serial.flush() when many advertisements arrive in dense
// environments. ESP32-S3/C5 UART supports much higher, but 460800 is the
// sweet spot for stable USB-CDC across hosts.
#define SERIAL_BAUD 460800

// ----- Firmware version -----
// Bumped manually; emitted in the device announce line at boot. Should
// match the version reported by the web flasher's manifest.json.
#define HUGINN_FW_VERSION "1.0"

// ----- Scan durations (ms) -----
#define WIFI_SCAN_DURATION   15000
#define BLE_SCAN_DURATION     8000
#define PINEAP_SCAN_DURATION 10000

// ----- Wardrive mode (per-channel scan + BLE alternation for moving captures) -----
// Cycle = WiFi phase (per-channel scans through a Marauder-style weighted
// schedule) + BLE phase. Phases are strictly exclusive on the shared 2.4 GHz
// radio.
//
// WiFi phase budget caps the channel-list iteration. With ~50–120 ms per
// channel and the schedule defined in scan_cycle.cpp:
//   S3  (~20 channels, 2.4 only) → ~2 s natural completion
//   C5  (~50 channels, dual-band) → ~5 s natural completion
// 8000 ms gives both room with margin and matters only as a hang-safety
// cap — under normal conditions the loop exits early when the schedule
// is exhausted.
//
// BLE phase length is unchanged — 1500 ms covers all 3 BLE ad channels
// with margin. Real cycle: ~3.5 s on S3, ~6.5 s on C5.
#define WARDRIVE_WIFI_DURATION_MS 8000
#define WARDRIVE_BLE_DURATION_MS  1500

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
