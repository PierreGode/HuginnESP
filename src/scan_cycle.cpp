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

// Wardrive channel schedule — Marauder-style weighted list. Busy channels
// appear multiple times per cycle so they're sampled ~4× more often than
// the rest. Each entry triggers a single-channel active scan of ~50–120 ms,
// emitting results immediately on completion — beats a full sweep (which
// blocks 1.5–3 s before any output) for catching APs that fade in and out
// while moving.
#ifdef HUGINN_BOARD_C5
static const uint8_t kWardriveChannels[] = {
    // 1st priority pass — high-traffic 2.4 + UNII non-DFS 5
    1, 6, 11,
    36, 40, 44, 48,
    149, 153, 157, 161,
    // 2nd priority pass — double-tap the busy ones
    1, 6, 11,
    36, 40, 44, 48,
    149, 153, 157, 161,
    // Full 2.4 GHz sweep (remaining channels)
    2, 3, 4, 5, 7, 8, 9, 10, 12, 13,
    // UNII-2A (DFS, passive scan still useful)
    52, 56, 60, 64,
    // UNII-2C (DFS)
    100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
    // UNII-3 / 6 GHz edge
    165, 169, 173, 177
};
#else
// S3: 2.4 GHz only — three passes of priority, then full sweep.
static const uint8_t kWardriveChannels[] = {
    1, 6, 11,
    1, 6, 11,
    1, 6, 11,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
};
#endif
static const size_t kWardriveChannelsLen =
    sizeof(kWardriveChannels) / sizeof(kWardriveChannels[0]);

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
            // Phase 1 — per-channel WiFi sweeps using the weighted schedule.
            // Each channel takes ~50–120 ms and emits its APs immediately,
            // so the host sees results streaming in throughout the phase
            // instead of waiting for a full sweep to complete. On-device
            // dedup suppresses re-emission of BSSIDs we've already sent in
            // this dedup window (reset on wardrive entry).
            if (activeBle != BLE_MODE_OFF) {
                ble_scanner_stop();
                activeBle = BLE_MODE_OFF;
            }
            wifi_scanner_set_dedup(true);

            uint32_t phaseStart = millis();
            size_t channelsDone = 0;
            while (g_manualOverride && g_currentMode == MODE_WARDRIVE
                   && channelsDone < kWardriveChannelsLen
                   && (millis() - phaseStart) < g_wardriveWifiMs) {
                uint8_t ch = kWardriveChannels[channelsDone++];
                wifi_scanner_start_channel(ch);
                // Wait for this channel's scan to complete. Per-channel
                // safety cap of 200 ms (dwell max=120 + setup overhead);
                // also bail if the phase deadline hits.
                uint32_t chStart = millis();
                while (g_manualOverride && g_currentMode == MODE_WARDRIVE) {
                    if ((millis() - chStart) > 200
                        || (millis() - phaseStart) >= g_wardriveWifiMs) {
                        wifi_scanner_stop();
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(5));
                    int16_t r = wifi_scanner_poll();
                    if (r >= 0 || r == -2 /*failed*/) {
                        wifi_scanner_process();
                        break;
                    }
                }
            }
            if (!(g_manualOverride && g_currentMode == MODE_WARDRIVE)) continue;
            Serial.printf("[CYCLE] wardrive WiFi phase: %u/%u channels in %ums\n",
                          (unsigned)channelsDone, (unsigned)kWardriveChannelsLen,
                          (unsigned)(millis() - phaseStart));

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
