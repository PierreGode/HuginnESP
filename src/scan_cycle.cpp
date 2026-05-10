#include "scan_cycle.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "serial_cmd.h"
#include "config.h"
#include "runtime_config.h"

static volatile bool s_running = true;

// Cycle definition: { mode, duration_ms }
struct CycleStep {
    ScanMode mode;
    uint32_t durationMs;
};

static const CycleStep CYCLE[] = {
    { MODE_WIFI,         WIFI_SCAN_DURATION   },  // Step 1
    { MODE_BLE_FILTERED, BLE_SCAN_DURATION    },  // Step 2
    { MODE_WIFI,         WIFI_SCAN_DURATION   },  // Step 3
    { MODE_BLE_ALL,      BLE_SCAN_DURATION    },  // Step 4
    { MODE_WIFI,         WIFI_SCAN_DURATION   },  // Step 5
    { MODE_SKIMMER,      BLE_SCAN_DURATION    },  // Step 6
    { MODE_WIFI,         WIFI_SCAN_DURATION   },  // Step 7
    { MODE_PINEAPPLE,    PINEAP_SCAN_DURATION },  // Step 8
};
static const int CYCLE_LEN = sizeof(CYCLE) / sizeof(CYCLE[0]);

void scan_cycle_init() {
    s_running = true;
}

void scan_cycle_pause() {
    s_running = false;
}

void scan_cycle_resume() {
    s_running = true;
}

bool scan_cycle_is_running() {
    return s_running;
}

void scan_cycle_task(void* param) {
    int step = 0;
    for (;;) {
        // Wardrive mode — tight WiFi/BLE alternation tuned for moving captures.
        // Stays in this branch until the user types `stop` (which clears
        // g_manualOverride and resets g_currentMode).
        if (g_manualOverride && g_currentMode == MODE_WARDRIVE) {
            wifi_scanner_start();
            uint32_t t = 0;
            while (t < g_wardriveWifiMs &&
                   g_manualOverride && g_currentMode == MODE_WARDRIVE) {
                vTaskDelay(pdMS_TO_TICKS(100));
                t += 100;
                wifi_scanner_process();
            }
            wifi_scanner_stop();

            if (!g_manualOverride || g_currentMode != MODE_WARDRIVE) continue;

            // BLE_ALL emits every device as JSON; Flipper / AirTag / skimmer
            // detections fire passively on the same stream.
            ble_scanner_start(BLE_MODE_ALL);
            t = 0;
            while (t < g_wardriveBleMs &&
                   g_manualOverride && g_currentMode == MODE_WARDRIVE) {
                vTaskDelay(pdMS_TO_TICKS(100));
                t += 100;
            }
            ble_scanner_stop();
            continue;
        }

        // If manual override is active (any other mode), sleep and retry.
        if (g_manualOverride || !s_running) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        const CycleStep& cs = CYCLE[step];
        g_currentMode = cs.mode;
        const uint32_t stepDurationMs =
            (cs.mode == MODE_WIFI) ? g_wifiScanDurationMs : cs.durationMs;

        switch (cs.mode) {
            case MODE_WIFI:
                wifi_scanner_start();
                break;
            case MODE_BLE_FILTERED:
                ble_scanner_start(BLE_MODE_FILTERED);
                break;
            case MODE_BLE_ALL:
                ble_scanner_start(BLE_MODE_ALL);
                break;
            case MODE_SKIMMER:
                ble_scanner_start(BLE_MODE_SKIMMER);
                break;
            case MODE_PINEAPPLE:
                wifi_scanner_check_pineapple();
                break;
            default:
                break;
        }

        // Wait for the duration, checking for abort every 250ms
        uint32_t elapsed = 0;
        while (elapsed < stepDurationMs) {
            vTaskDelay(pdMS_TO_TICKS(250));
            elapsed += 250;

            // If WiFi scan, process results as they arrive
            if (cs.mode == MODE_WIFI) {
                wifi_scanner_process();
            }

            // Abort if manual override engaged
            if (g_manualOverride) break;
        }

        // Stop current scan before moving to next
        if (!g_manualOverride) {
            if (cs.mode == MODE_WIFI || cs.mode == MODE_PINEAPPLE) {
                wifi_scanner_stop();
            } else {
                ble_scanner_stop();
            }
        }

        step = (step + 1) % CYCLE_LEN;
    }
}
