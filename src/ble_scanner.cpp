#include "ble_scanner.h"
#include "runtime_config.h"
#include "skimmer_led.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <set>

static BleMode s_mode = BLE_MODE_OFF;
static int s_bleCount = 0;
static int s_skimmerCount = 0;
static BLEScan* s_pScan = nullptr;

// ---- Session totals — unique MACs observed since boot ----
// Touched by the BLE callback (BLE task) and by the display task on the
// other core, so all access goes through s_sessionMutex.
#define BLE_SESSION_TRACK_CAP 4096
static SemaphoreHandle_t s_sessionMutex = nullptr;
static std::set<String> s_sessionBleMacs;
static std::set<String> s_sessionSkimmerMacs;

// ---- Skimmer detection ----
static bool isSkimmerDevice(BLEAdvertisedDevice& dev) {
    String name = String(dev.getName().c_str());
    return isSkimmerName(name);
}

// ---- Scan callbacks ----
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        String mac  = String(advertisedDevice.getAddress().toString().c_str());
        int rssi    = advertisedDevice.getRSSI();
        bool skimmer = isSkimmerDevice(advertisedDevice);

        // Session totals — unique MACs since boot (capped to bound memory).
        if (s_sessionMutex && xSemaphoreTake(s_sessionMutex, portMAX_DELAY) == pdTRUE) {
            if (s_sessionBleMacs.size() < BLE_SESSION_TRACK_CAP) s_sessionBleMacs.insert(mac);
            if (skimmer) s_sessionSkimmerMacs.insert(mac);
            xSemaphoreGive(s_sessionMutex);
        }
        s_bleCount++;

        if (s_mode != BLE_MODE_SKIMMER || !skimmer) return;

        s_skimmerCount++;
        skimmer_led_notify(rssi, SKIMMER_LED_SKIMMER);
    }
};

static ScanCallbacks s_callbacks;

void ble_scanner_init() {
    if (!s_sessionMutex) s_sessionMutex = xSemaphoreCreateMutex();
    BLEDevice::init("SkimGuard-C5");
    s_pScan = BLEDevice::getScan();
    s_pScan->setAdvertisedDeviceCallbacks(&s_callbacks, false);
    s_pScan->setActiveScan(true);
    // Aggressive active scan settings for maximum detection responsiveness.
    s_pScan->setInterval(80);
    s_pScan->setWindow(79);
}

void ble_scanner_start(BleMode mode) {
    if (s_mode != BLE_MODE_OFF) ble_scanner_stop();
    s_mode = (mode == BLE_MODE_OFF) ? BLE_MODE_OFF : BLE_MODE_SKIMMER;
    s_bleCount = 0;
    s_skimmerCount = 0;
    s_pScan->clearResults();
    // Non-blocking: pass a no-op callback so start() returns immediately.
    // Duration 0 = indefinite; scan_cycle stops it explicitly via ble_scanner_stop().
    s_pScan->start(0, [](BLEScanResults){}, false);
}

void ble_scanner_stop() {
    if (s_pScan && s_mode != BLE_MODE_OFF) {
        s_pScan->stop();
    }
    s_mode = BLE_MODE_OFF;
}

int ble_scanner_count() {
    return s_bleCount;
}

int ble_scanner_skimmer_count() {
    return s_skimmerCount;
}

static int sessionSetSize(const std::set<String>& s) {
    if (!s_sessionMutex) return 0;
    int n = 0;
    if (xSemaphoreTake(s_sessionMutex, portMAX_DELAY) == pdTRUE) {
        n = (int)s.size();
        xSemaphoreGive(s_sessionMutex);
    }
    return n;
}

int ble_scanner_session_count()         { return sessionSetSize(s_sessionBleMacs); }
int ble_scanner_session_skimmer_count() { return sessionSetSize(s_sessionSkimmerMacs); }
