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
        // Tight WiFi ↔ BLE alternation per the README. Running BLE_MODE_ALL
        // in parallel with the WiFi scan starves WiFi: the BLE scanner sets
        // interval=100 / window=99 which owns the shared 2.4 GHz radio ~99 %
        // of the time, so WiFi completes with apCount=0. The fix is to give
        // each radio an exclusive window per cycle.
        if (g_manualOverride && g_currentMode == MODE_WARDRIVE) {
            // Phase 1 — WiFi (BLE off so the radio is exclusively WiFi).
            // Chain back-to-back scans within the WiFi phase: each
            // completed sweep emits its full set of records, then we
            // immediately start another. This uses the whole phase
            // budget instead of going idle after a single sweep, which
            // is the difference between one and three sweeps per cycle
            // on the S3 (and one full sweep that actually completes
            // instead of being aborted, on the C5).
            if (activeBle != BLE_MODE_OFF) {
                ble_scanner_stop();
                activeBle = BLE_MODE_OFF;
            }
            uint32_t phaseStart = millis();
            int wardriveSweeps = 0;
            bool deadlineHit = false;
            while (g_manualOverride && g_currentMode == MODE_WARDRIVE
                   && !deadlineHit
                   && (millis() - phaseStart) < g_wardriveWifiMs) {
                wifi_scanner_start();
                // Wait for this sweep to complete or the phase deadline,
                // whichever comes first. On natural completion, process
                // and start the next sweep. On deadline, abort cleanly.
                while (g_manualOverride && g_currentMode == MODE_WARDRIVE) {
                    if ((millis() - phaseStart) >= g_wardriveWifiMs) {
                        wifi_scanner_stop();
                        deadlineHit = true;
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(50));
                    int16_t r = wifi_scanner_poll();
                    if (r >= 0 || r == -2 /*failed*/) {
                        wifi_scanner_process();
                        wardriveSweeps++;
                        break;
                    }
                }
            }
            if (!(g_manualOverride && g_currentMode == MODE_WARDRIVE)) continue;
            Serial.printf("[CYCLE] wardrive WiFi phase: %d sweep(s) in %ums\n",
                          wardriveSweeps, (unsigned)(millis() - phaseStart));

            // Phase 2 — BLE_MODE_ALL for the configured window. Flipper /
            // AirTag / skimmer detections still fire passively from the
            // same stream.
            ble_scanner_start(BLE_MODE_ALL);
            activeBle = BLE_MODE_ALL;
            uint32_t bleT = 0;
            while (g_manualOverride && g_currentMode == MODE_WARDRIVE
                   && bleT < g_wardriveBleMs) {
                vTaskDelay(pdMS_TO_TICKS(50));
                bleT += 50;
            }
            ble_scanner_stop();
            activeBle = BLE_MODE_OFF;
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
