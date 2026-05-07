#ifndef WIFI_SCANNER_H
#define WIFI_SCANNER_H

#include <Arduino.h>

// Detected WiFi network record
struct WifiNetwork {
    String ssid;
    String bssid;
    int32_t rssi;
    int32_t channel;
    String security;
};

void wifi_scanner_init();

// Start an async WiFi scan. Results are printed to Serial
// when wifi_scanner_process() is called.
void wifi_scanner_start();

// Process completed scan results — prints to Serial and
// updates the shared network list.
void wifi_scanner_process();

// Stop / abort current scan.
void wifi_scanner_stop();

// Return the count of networks found in the last completed scan.
int wifi_scanner_count();

// Return pointer to internal network list and its size.
const WifiNetwork* wifi_scanner_get_networks(int& count);

// Pineapple / Evil-Twin detection across cached networks.
void wifi_scanner_check_pineapple();

#endif // WIFI_SCANNER_H
