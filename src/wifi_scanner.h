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

// Start an async WiFi scan across all channels. Results are printed
// to Serial when wifi_scanner_process() is called.
void wifi_scanner_start();

// Start an async WiFi scan on a single channel (1-14 for 2.4 GHz,
// 36-177 for 5 GHz). Used by wardrive mode for per-channel sweeps
// with a weighted schedule — yields APs ~80 ms after the call
// instead of waiting 1.5–3 s for a full sweep.
void wifi_scanner_start_channel(uint8_t channel);

// On-device BSSID dedup. When enabled, wifi_scanner_process() skips
// emitting BSSIDs already seen in this dedup window. Used by wardrive
// mode to suppress duplicate emissions across many per-channel scans —
// each AP gets emitted once until reset. Ragnar's upsert handles
// dedup on the host too, but on-device dedup saves serial bytes and
// reduces host parse load.
void wifi_scanner_set_dedup(bool enabled);
void wifi_scanner_reset_dedup();

// Poll scan state. Mirrors WiFi.scanComplete() semantics:
//   -1  -> still running
//   -2  -> failed (WIFI_SCAN_FAILED)
//    0+ -> done, call wifi_scanner_process() to read records
int16_t wifi_scanner_poll();

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

// Session total — unique BSSIDs observed since boot. RAM-only, reset on power cycle.
int wifi_scanner_session_count();

#endif // WIFI_SCANNER_H
