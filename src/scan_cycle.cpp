#include "scan_cycle.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "serial_cmd.h"
#include "config.h"
#include "runtime_config.h"

static volatile bool s_running = true;

// BLE modes to rotate through during normal cycling.
static const BleMode BLE_ROTATION[] = {
    BLE_MODE_FILTERED, BLE_MODE_ALL, BLE_MODE_SKIMMER
};
static const int BLE_ROTATION_LEN = 3;

// Run a pineapple check every this many WiFi scans.
static const int PINEAPPLE_EVERY_N = 4;

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

// Poll until the async WiFi scan finishes, then process results.
// Returns immediately if manual override is engaged.
static void wifi_wait_and_process(uint32_t timeoutMs) {
    uint32_t elapsed = 0;
    while (elapsed < timeoutMs && !g_manualOverride) {
        vTaskDelay(pdMS_TO_TICKS(50));
        elapsed += 50;
        int16_t r = wifi_scanner_poll();
        if (r >= 0 || r == -2 /*failed*/) {
            wifi_scanner_process();
            return;
        }
    }
    wifi_scanner_stop();
}

void scan_cycle_task(void* param) {
    int  bleModeIdx   = 0;
    int  wifiScans    = 0;
    BleMode activeBle = BLE_MODE_OFF;

    for (;;) {
        // ── Wardrive mode ────────────────────────────────────────────────
        // BLE_ALL runs continuously while WiFi scans loop back-to-back.
        if (g_manualOverride && g_currentMode == MODE_WARDRIVE) {
            if (activeBle != BLE_MODE_ALL) {
                ble_scanner_start(BLE_MODE_ALL);
                activeBle = BLE_MODE_ALL;
            }
            wifi_scanner_start();
            uint32_t t = 0;
            while (g_manualOverride && g_currentMode == MODE_WARDRIVE) {
                vTaskDelay(pdMS_TO_TICKS(50));
                t += 50;
                int16_t r = wifi_scanner_poll();
                if (r >= 0 || r == -2 /*failed*/) {
                    wifi_scanner_process();
                    break;
                }
                if (t >= g_wardriveWifiMs) {
                    wifi_scanner_stop();
                    break;
                }
            }
            continue;
        }

        // ── Manual override (non-wardrive) or paused ─────────────────────
        if (g_manualOverride || !s_running) {
            if (activeBle != BLE_MODE_OFF) {
                ble_scanner_stop();
                activeBle = BLE_MODE_OFF;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ── Periodic pineapple check ──────────────────────────────────────
        // Stop BLE so the dedicated WiFi scan gets clean radio access.
        if (wifiScans > 0 && (wifiScans % PINEAPPLE_EVERY_N) == 0) {
            if (activeBle != BLE_MODE_OFF) {
                ble_scanner_stop();
                activeBle = BLE_MODE_OFF;
            }
            g_currentMode = MODE_PINEAPPLE;
            wifi_scanner_check_pineapple();
            wifiScans++; // prevent immediate re-trigger
            continue;    // restart loop — BLE and WiFi restart next iteration
        }

        // ── Normal cycle: WiFi + BLE simultaneously ───────────────────────
        // Rotate BLE mode each WiFi scan; ESP32 coexistence shares the radio.
        BleMode nextBle = BLE_ROTATION[bleModeIdx];
        if (activeBle != nextBle) {
            ble_scanner_start(nextBle); // stops old mode, starts new one
            activeBle = nextBle;
        }

        g_currentMode = MODE_WIFI;
        Serial.printf("[CYCLE] WiFi scan #%d starting (BLE mode %d)\n", wifiScans, (int)nextBle);
        wifi_scanner_start();
        wifi_wait_and_process(g_wifiScanDurationMs);
        Serial.printf("[CYCLE] WiFi scan #%d done, count=%d\n", wifiScans, wifi_scanner_count());

        wifiScans++;
        bleModeIdx = (bleModeIdx + 1) % BLE_ROTATION_LEN;
    }
}
