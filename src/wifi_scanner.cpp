#include "wifi_scanner.h"
#include "config.h"
#include "runtime_config.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>
#include <map>
#include <set>

static std::vector<WifiNetwork> s_networks;
static volatile bool s_scanning   = false;
static volatile bool s_scanDone   = false;
static volatile bool s_scanFailed = false;
static int  s_lastCount = 0;

// Map SSID -> list of BSSIDs for evil-twin detection.
static std::map<String, std::vector<String>> s_ssidMap;

// Session total — unique BSSIDs observed since boot.
// Read by the display task on the other core, written here.
#define WIFI_SESSION_TRACK_CAP 4096
static SemaphoreHandle_t s_sessionMutex = nullptr;
static std::set<String>  s_sessionBssids;

static const char* authModeStr(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN:            return "Open";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        default:                        return "Unknown";
    }
}

static void onScanEvent(arduino_event_t* event) {
    if (event->event_id == ARDUINO_EVENT_WIFI_SCAN_DONE) {
        if (event->event_info.wifi_scan_done.status == 0) {
            s_scanDone = true;
        } else {
            s_scanFailed = true;
        }
    }
}

void wifi_scanner_init() {
    if (!s_sessionMutex) s_sessionMutex = xSemaphoreCreateMutex();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.onEvent(onScanEvent, ARDUINO_EVENT_WIFI_SCAN_DONE);
    Serial.printf("[WIFI] init done, mode=%d\n", WiFi.getMode());
}

void wifi_scanner_start() {
    if (s_scanning) return;
    s_scanning   = true;
    s_scanDone   = false;
    s_scanFailed = false;
    s_networks.clear();

    wifi_scan_config_t cfg = {};
    cfg.show_hidden = false;
    cfg.scan_type   = WIFI_SCAN_TYPE_ACTIVE;
    // Arduino's WiFi.scanNetworks wrapper hardcodes min=100/max=120 ms
    // per channel, which on a dual-band radio (≈50 channels incl. DFS)
    // pushes a full sweep past 10 s. We scan every channel on every
    // pass (mandatory for wardriving at 60 km/h — an AP is only in
    // range for a few seconds), but with much tighter dwell windows:
    // probe responses arrive in well under 80 ms, and the low min
    // lets quiet channels drop through almost immediately. Net effect
    // on the C5 is a full dual-band sweep in ≈2–3 s instead of ≈15 s.
    cfg.scan_time.active.min = 20;
    cfg.scan_time.active.max = 80;

    esp_err_t err = esp_wifi_scan_start(&cfg, /*block=*/false);
    if (err != ESP_OK) {
        Serial.printf("[WIFI] scan_start FAILED: 0x%x (%s)\n", err, esp_err_to_name(err));
        s_scanning   = false;
        s_scanFailed = true;
    } else {
        Serial.println("[WIFI] scan_start OK (async)");
    }
}

int16_t wifi_scanner_poll() {
    if (s_scanFailed) return -2;
    if (s_scanDone)   return 0;
    return -1;
}

void wifi_scanner_process() {
    if (s_scanFailed) {
        Serial.println("[WIFI] process: scan had failed, resetting");
        s_scanning   = false;
        s_scanFailed = false;
        return;
    }
    if (!s_scanDone) return;

    uint16_t apCount = 0;
    esp_wifi_scan_get_ap_num(&apCount);
    std::vector<wifi_ap_record_t> records(apCount);
    if (apCount > 0) {
        esp_wifi_scan_get_ap_records(&apCount, records.data());
    }

    s_ssidMap.clear();
    s_networks.clear();
    s_lastCount = apCount;

    char bssidStr[18];
    for (uint16_t i = 0; i < apCount; i++) {
        const wifi_ap_record_t& r = records[i];
        snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 r.bssid[0], r.bssid[1], r.bssid[2], r.bssid[3], r.bssid[4], r.bssid[5]);

        WifiNetwork net;
        net.ssid     = (const char*)r.ssid;
        net.bssid    = bssidStr;
        net.rssi     = r.rssi;
        net.channel  = r.primary;
        net.security = authModeStr(r.authmode);
        s_networks.push_back(net);

        // JSON output for Ragnar parser
        Serial.printf("{\"type\":\"WIFI\",\"mac\":\"%s\",\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d,\"auth\":\"%s\"}\n",
                      net.bssid.c_str(), net.ssid.c_str(), net.rssi, net.channel, net.security.c_str());
        Serial.flush();

        // Track for evil-twin detection
        s_ssidMap[net.ssid].push_back(net.bssid);

        // Session total — unique BSSIDs since boot (capped to bound memory).
        if (s_sessionMutex && xSemaphoreTake(s_sessionMutex, portMAX_DELAY) == pdTRUE) {
            if (s_sessionBssids.size() < WIFI_SESSION_TRACK_CAP) {
                s_sessionBssids.insert(net.bssid);
            }
            xSemaphoreGive(s_sessionMutex);
        }
    }

    s_scanning = false;
    s_scanDone = false;
}

void wifi_scanner_stop() {
    if (s_scanning) {
        esp_wifi_scan_stop();
        s_scanning   = false;
        s_scanDone   = false;
        s_scanFailed = false;
    }
}

int wifi_scanner_count() {
    return s_lastCount;
}

const WifiNetwork* wifi_scanner_get_networks(int& count) {
    count = (int)s_networks.size();
    return count > 0 ? s_networks.data() : nullptr;
}

int wifi_scanner_session_count() {
    if (!s_sessionMutex) return 0;
    int n = 0;
    if (xSemaphoreTake(s_sessionMutex, portMAX_DELAY) == pdTRUE) {
        n = (int)s_sessionBssids.size();
        xSemaphoreGive(s_sessionMutex);
    }
    return n;
}

void wifi_scanner_check_pineapple() {
    // Run a fresh scan synchronously for pineapple check
    wifi_scanner_start();

    // Wait for scan to complete (blocking, used during pineapple cycle step)
    unsigned long start = millis();
    while (!s_scanDone && !s_scanFailed) {
        if (millis() - start > g_wifiScanDurationMs) {
            wifi_scanner_stop();
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    wifi_scanner_process();

    // Check for suspicious duplicate SSIDs — not just any mesh/repeater setup.
    // A real pineapple/evil-twin is indicated by:
    //   1. Same SSID but MIXED security (e.g. one Open + one WPA2)
    //   2. Same SSID with an Open AP cloning a known encrypted network
    // Normal mesh/repeater setups share SSID + same security = NOT suspicious.
    for (const auto& pair : s_ssidMap) {
        if (pair.second.size() <= 1) continue;
        if (pair.first.length() == 0) continue; // skip hidden SSIDs

        // Collect security types for this SSID across all BSSIDs
        bool hasOpen = false;
        bool hasEncrypted = false;
        for (const auto& bssid : pair.second) {
            for (const auto& net : s_networks) {
                if (net.bssid == bssid) {
                    if (String(net.security) == "Open") {
                        hasOpen = true;
                    } else {
                        hasEncrypted = true;
                    }
                    break;
                }
            }
        }

        // Only alert if there's a mix of Open + Encrypted for the same SSID
        // This is the hallmark of an evil-twin / pineapple attack
        if (hasOpen && hasEncrypted) {
            for (size_t i = 0; i < pair.second.size(); i++) {
                Serial.printf("Pineapple detected: %s\n", pair.first.c_str());
                Serial.printf("BSSID: %s\n", pair.second[i].c_str());
                for (const auto& net : s_networks) {
                    if (net.bssid == pair.second[i]) {
                        Serial.printf("Channel: %d\n", net.channel);
                        Serial.printf("Security: %s\n", net.security.c_str());
                        break;
                    }
                }
            }
        }
    }
}
