#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <Arduino.h>

// BLE scan modes
enum BleMode {
    BLE_MODE_OFF,
    BLE_MODE_ALL,
    BLE_MODE_FILTERED,
    BLE_MODE_SKIMMER
};

struct BleDevice {
    String mac;
    String name;
    int32_t rssi;
    bool isSkimmer;
};

void ble_scanner_init();
void ble_scanner_start(BleMode mode);
void ble_scanner_stop();
int  ble_scanner_count();
int  ble_scanner_skimmer_count();

// Session totals — unique MACs observed since boot. RAM-only, reset on power cycle.
int  ble_scanner_session_count();
int  ble_scanner_session_skimmer_count();

#endif // BLE_SCANNER_H
