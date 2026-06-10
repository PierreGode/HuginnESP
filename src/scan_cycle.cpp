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

#ifdef HUGINN_BOARD_C5
static const uint8_t kWardriveChannels[] = {
    1, 6, 11,
    36, 40, 44, 48,
    149, 153, 157, 161,
    1, 6, 11,
    36, 40, 44, 48,
    149, 153, 157, 161,
    2, 3, 4, 5, 7, 8, 9, 10, 12, 13,
    52, 56, 60, 64,
    100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
    165, 169, 173, 177
};
#else
static const uint8_t kWardriveChannels[] = {
    1, 6, 11,
    1, 6, 11,
    1, 6, 11,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
};
#endif
static const size_t kWardriveChannelsLen =
    sizeof(kWardriveChannels) / sizeof(kWardriveChannels[0]);

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
        vTaskDelay(pdMS_TO_TICKS(20));
        elapsed += 20;
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
        if (g_manualOverride && g_currentMode == MODE_WARDRIVE) {
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
                uint32_t chStart = millis();
                while (g_manualOverride && g_currentMode == MODE_WARDRIVE) {
                    if ((millis() - chStart) > 200
                        || (millis() - phaseStart) >= g_wardriveWifiMs) {
                        wifi_scanner_stop();
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(5));
                    int16_t r = wifi_scanner_poll();
                    if (r >= 0 || r == -2) {
                        wifi_scanner_process();
                        break;
                    }
                }
            }
            if (!(g_manualOverride && g_currentMode == MODE_WARDRIVE)) continue;
            Serial.printf("[CYCLE] wardrive WiFi phase: %u/%u channels in %ums\n",
                          (unsigned)channelsDone, (unsigned)kWardriveChannelsLen,
                          (unsigned)(millis() - phaseStart));

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

        // ── Skimmer-only mode ────────────────────────────────────────────
        // First-class persistent branch (like wardrive) so the BLE skimmer
        // scan keeps running continuously instead of being parked by the
        // generic manual-override idle branch below.
        if (g_manualOverride && g_currentMode == MODE_SKIMMER) {
            if (activeBle != BLE_MODE_SKIMMER) {
                wifi_scanner_stop();
                ble_scanner_start(BLE_MODE_SKIMMER);
                activeBle = BLE_MODE_SKIMMER;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // ── Manual override (non-wardrive) or paused ─────────────────────
        if (g_manualOverride || !s_running) {
            if (activeBle != BLE_MODE_OFF) {
                ble_scanner_stop();
                activeBle = BLE_MODE_OFF;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // ── Periodic pineapple check ──────────────────────────────────────
        // Stop BLE so the dedicated WiFi scan gets clean radio access.
        if (g_pineappleEveryN > 0 && wifiScans > 0 && (wifiScans % (int)g_pineappleEveryN) == 0) {
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
