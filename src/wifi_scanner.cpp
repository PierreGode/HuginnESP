#include "wifi_scanner.h"
#include "config.h"
#include <WiFi.h>
#include <vector>
#include <map>

static std::vector<WifiNetwork> s_networks;
static bool s_scanning = false;
static int  s_lastCount = 0;

// Map SSID -> list of BSSIDs for evil-twin detection
static std::map<String, std::vector<String>> s_ssidMap;

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
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
}

void wifi_scanner_start() {
    if (s_scanning) return;
    s_scanning = true;
    s_networks.clear();
    WiFi.scanNetworks(true); // async = true
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

void wifi_scanner_check_pineapple() {
    // Run a fresh scan synchronously for pineapple check
    wifi_scanner_start();

    // Wait for scan to complete (blocking, used during pineapple cycle step)
    unsigned long start = millis();
    while (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
        if (millis() - start > WIFI_SCAN_DURATION) break;
        delay(100);
    }
    wifi_scanner_process();

    // Check for duplicate SSIDs with different BSSIDs
    for (const auto& pair : s_ssidMap) {
        if (pair.second.size() > 1) {
            for (size_t i = 0; i < pair.second.size(); i++) {
                Serial.printf("Pineapple detected: %s\n", pair.first.c_str());
                Serial.printf("BSSID: %s\n", pair.second[i].c_str());
                // Find channel for this BSSID
                for (const auto& net : s_networks) {
                    if (net.bssid == pair.second[i]) {
                        Serial.printf("Channel: %d\n", net.channel);
                        break;
                    }
                }
            }
        }
    }
}
