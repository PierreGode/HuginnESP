#ifndef CONFIG_H
#define CONFIG_H

// ----- Serial -----
#define SERIAL_BAUD 460800

// ----- Firmware version -----
// Bumped manually; emitted in the device announce line at boot. Should
// match the version reported by the web flasher's manifest.json.
#define HUGINN_FW_VERSION "1.0"

// ----- Scan durations (ms) -----
#define WIFI_SCAN_DURATION    8000
#define BLE_SCAN_DURATION     8000
#define PINEAP_SCAN_DURATION 10000

// Run periodic pineapple/evil-twin check every N completed WiFi scans.
// Set to 0 to disable periodic checks (manual `pineap` command still works).
#define PINEAPPLE_EVERY_N_DEFAULT 8

// ----- Wardrive mode -----
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

// ----- GPS (optional — enabled by -DHUGINN_HAS_GPS=1 build flag) -----
// Override any of these from platformio.ini build_flags to match your board's
// free GPIO pins. TX is declared but most receive-only modules leave it unconnected.
#if HUGINN_HAS_GPS
#ifndef GPS_UART_NUM
#define GPS_UART_NUM   1
#endif
#ifndef GPS_RX_PIN
#if HUGINN_BOARD_C5 && HUGINN_BOARD_XIAO_C5
#define GPS_RX_PIN    12
#else
#define GPS_RX_PIN    17
#endif
#endif
#ifndef GPS_TX_PIN
#if HUGINN_BOARD_C5 && HUGINN_BOARD_XIAO_C5
#define GPS_TX_PIN     1
#else
#define GPS_TX_PIN    18
#endif
#endif
#define GPS_BAUD       9600
#define GPS_TASK_STACK 4096
#endif

// ----- Skimmer suspicious names (defaults; runtime list lives in runtime_config) -----
static const char* SKIMMER_NAMES_DEFAULT[] = {
    "HC-05", "HC-06", "HC-08",
    "BT05", "BT06",
    "JDY-30", "JDY-31", "JDY-33",
    "SPP-CA",
    nullptr
};

// ----- Skimmer proximity LED (optional — enabled by -DHUGINN_HAS_SKIMMER_LED=1) -----
// On boards with an addressable RGB "white" LED (e.g. the ESP32-C5 DevKitC
// RGB_BUILTIN), the LED blinks while a potential skimmer is in range and blinks
// faster the closer it is (stronger RSSI). Driven with the Arduino core's
// rgbLedWrite(). Override the pin with -DSKIMMER_LED_PIN=<gpio> if your board
// wires the LED elsewhere.
#if HUGINN_HAS_SKIMMER_LED
#ifndef SKIMMER_LED_PIN
#ifdef RGB_BUILTIN
#define SKIMMER_LED_PIN        RGB_BUILTIN
#else
#define SKIMMER_LED_PIN        LED_BUILTIN
#endif
#endif
#ifndef SKIMMER_LED_BRIGHTNESS
#define SKIMMER_LED_BRIGHTNESS 40      // 0-255 white level when lit (these LEDs are bright)
#endif
// RSSI is negative; closer ≈ nearer 0 (e.g. -45), farther ≈ more negative (-95).
#define SKIMMER_LED_RSSI_NEAR  -45     // at/above this → fastest blink
#define SKIMMER_LED_RSSI_FAR   -95     // at/below this → slowest blink
#define SKIMMER_LED_FAST_MS     70     // blink half-period at closest range
#define SKIMMER_LED_SLOW_MS   1000     // blink half-period at farthest range
#define SKIMMER_LED_HOLD_MS   4000     // keep blinking this long after the last sighting
#define SKIMMER_LED_TASK_STACK 2048
#endif

// ----- Flipper Zero BLE identification -----
// Flipper Zero manufacturer data company ID (0x4C01 is the placeholder;
// real identification uses service UUID + manufacturer data heuristics)
#define FLIPPER_SERVICE_UUID "8e400001-f315-4f60-9fb8-838830daea50"

// ----- AirTag identification -----
// Apple company ID in BLE manufacturer data
#define APPLE_COMPANY_ID 0x004C

#endif // CONFIG_H
