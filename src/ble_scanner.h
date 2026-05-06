#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <Arduino.h>

// BLE scan modes
enum BleMode {
    BLE_MODE_OFF,
    BLE_MODE_ALL,
    BLE_MODE_FILTERED,   // Flipper + AirTag only
    BLE_MODE_SKIMMER
};

struct BleDevice {
    String mac;
    String name;
    int32_t rssi;
    bool isFlipper;
    bool isAirTag;
    bool isSkimmer;
    uint8_t flipperColor; // 0=White,1=Black,2=Transparent
};

void ble_scanner_init();
void ble_scanner_start(BleMode mode);
void ble_scanner_stop();
int  ble_scanner_count();

#endif // BLE_SCANNER_H
