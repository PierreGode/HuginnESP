#include "wifi_scanner.h"
#include "config.h"
#include "runtime_config.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>
#include <map>
#include <set>

static std::vector<WifiNetwork> s_networks;
static bool s_scanning = false;
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

void wifi_scanner_init() {
    if (!s_sessionMutex) s_sessionMutex = xSemaphoreCreateMutex();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
}

void wifi_scanner_start() {
    if (s_scanning) return;
    s_scanning = true;
    s_networks.clear();
    // 120 ms per channel is the minimum for reliable beacon capture;
    // cuts scan time ~2.5× vs the 300 ms default.
    WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false,
                      /*passive=*/false, /*max_ms_per_chan=*/120);
}

void wifi_scanner_process() {
    int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    if (n == WIFI_SCAN_FAILED) {
        s_scanning = false;
        return;
    }

    s_ssidMap.clear();
    s_networks.clear();
    s_lastCount = n;

    for (int i = 0; i < n; i++) {
        WifiNetwork net;
        net.ssid     = WiFi.SSID(i);
        net.bssid    = WiFi.BSSIDstr(i);
        net.rssi     = WiFi.RSSI(i);
        net.channel  = WiFi.channel(i);
        net.security = authModeStr(WiFi.encryptionType(i));
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

    WiFi.scanDelete();
    s_scanning = false;
}

void wifi_scanner_stop() {
    if (s_scanning) {
        WiFi.scanDelete();
        s_scanning = false;
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
    while (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
        if (millis() - start > g_wifiScanDurationMs) break;
        vTaskDelay(pdMS_TO_TICKS(100));
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
