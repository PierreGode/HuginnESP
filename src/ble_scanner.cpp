#include "ble_scanner.h"
#include "config.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <map>

static BleMode s_mode = BLE_MODE_OFF;
static int s_bleCount = 0;
static BLEScan* s_pScan = nullptr;

// Spam detection: MAC -> ad count within window
static std::map<String, uint32_t> s_adCounts;
static unsigned long s_spamWindowStart = 0;

// ---- Flipper Zero detection heuristics ----
static bool isFlipperDevice(BLEAdvertisedDevice& dev, uint8_t& color) {
    // Check for Flipper Zero service UUID
    if (dev.isAdvertisingService(BLEUUID(FLIPPER_SERVICE_UUID))) {
        color = 0;
        String name = String(dev.getName().c_str());
        if (name.indexOf("Flipper") >= 0) {
            if (name.indexOf("Black") >= 0)       color = 1;
            else if (name.indexOf("Trans") >= 0)   color = 2;
        }
        return true;
    }

    // Heuristic: check device name
    String name = String(dev.getName().c_str());
    if (name.startsWith("Flipper")) {
        color = 0;
        if (name.indexOf("Black") >= 0)       color = 1;
        else if (name.indexOf("Trans") >= 0)   color = 2;
        return true;
    }
    return false;
}

// ---- AirTag detection ----
static bool isAirTagDevice(BLEAdvertisedDevice& dev) {
    if (!dev.haveManufacturerData()) return false;
    std::string mfr = dev.getManufacturerData();
    if (mfr.size() < 2) return false;

    uint16_t companyId = (uint16_t)(uint8_t)mfr[0] | ((uint16_t)(uint8_t)mfr[1] << 8);
    if (companyId != APPLE_COMPANY_ID) return false;

    if (mfr.size() >= 3) {
        uint8_t typeByte = (uint8_t)mfr[2];
        if (typeByte == 0x12 || typeByte == 0x07) {
            return true;
        }
    }
    return false;
}

// ---- Skimmer detection ----
static bool isSkimmerDevice(BLEAdvertisedDevice& dev) {
    String name = String(dev.getName().c_str());
    if (name.length() == 0) return false;
    for (int i = 0; SKIMMER_NAMES[i] != nullptr; i++) {
        if (name.equalsIgnoreCase(SKIMMER_NAMES[i])) {
            return true;
        }
    }
    return false;
}

// ---- BLE Spam detection ----
static void checkSpam(const String& mac) {
    unsigned long now = millis();
    if (now - s_spamWindowStart > BLE_SPAM_WINDOW_MS) {
        s_adCounts.clear();
        s_spamWindowStart = now;
    }
    s_adCounts[mac]++;
    if (s_adCounts[mac] == BLE_SPAM_THRESHOLD) {
        Serial.printf("BLE Spam detected from %s\n", mac.c_str());
    }
}

// ---- Scan callbacks ----
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        String mac  = String(advertisedDevice.getAddress().toString().c_str());
        String name = String(advertisedDevice.getName().c_str());
        int rssi    = advertisedDevice.getRSSI();

        // Always check for spam
        checkSpam(mac);

        uint8_t flipperColor = 0;
        bool flipper = isFlipperDevice(advertisedDevice, flipperColor);
        bool airtag  = isAirTagDevice(advertisedDevice);
        bool skimmer = isSkimmerDevice(advertisedDevice);

        // ---- Filtered mode: only output Flipper / AirTag ----
        if (s_mode == BLE_MODE_FILTERED) {
            if (flipper) {
                const char* colorStr = "White";
                if (flipperColor == 1) colorStr = "Black";
                else if (flipperColor == 2) colorStr = "Transparent";

                Serial.printf("Found %s Flipper Device:\n", colorStr);
                Serial.printf("MAC: %s,\n", mac.c_str());
                Serial.printf("Name: %s,\n", name.c_str());
                Serial.printf("RSSI: %d\n", rssi);
            }
            if (airtag) {
                static int tagNum = 0;
                tagNum++;
                Serial.println("AirTag found!");
                Serial.printf("Tag: %d\n", tagNum);
                Serial.printf("MAC Address: %s\n", mac.c_str());
                Serial.printf("RSSI: %d\n", rssi);
            }
            s_bleCount++;
            return;
        }

        // ---- Skimmer mode ----
        if (s_mode == BLE_MODE_SKIMMER) {
            if (skimmer) {
                Serial.println("POTENTIAL SKIMMER DETECTED!");
                Serial.printf("Device Name: %s\n", name.c_str());
                Serial.printf("MAC Address: %s\n", mac.c_str());
                Serial.printf("RSSI: %d\n", rssi);
                Serial.println("Reason: Suspicious BLE module near payment terminal");
            }
            s_bleCount++;
            return;
        }

        // ---- All mode: output everything ----
        if (s_mode == BLE_MODE_ALL) {
            Serial.printf("MAC: %s, Name: %s, RSSI: %d\n", mac.c_str(),
                          name.length() > 0 ? name.c_str() : "(unknown)", rssi);

            if (flipper) {
                const char* colorStr = "White";
                if (flipperColor == 1) colorStr = "Black";
                else if (flipperColor == 2) colorStr = "Transparent";
                Serial.printf("Found %s Flipper Device:\n", colorStr);
                Serial.printf("MAC: %s,\n", mac.c_str());
                Serial.printf("Name: %s,\n", name.c_str());
                Serial.printf("RSSI: %d\n", rssi);
            }
            if (airtag) {
                static int tagAll = 0;
                tagAll++;
                Serial.println("AirTag found!");
                Serial.printf("Tag: %d\n", tagAll);
                Serial.printf("MAC Address: %s\n", mac.c_str());
                Serial.printf("RSSI: %d\n", rssi);
            }
            if (skimmer) {
                Serial.println("POTENTIAL SKIMMER DETECTED!");
                Serial.printf("Device Name: %s\n", name.c_str());
                Serial.printf("MAC Address: %s\n", mac.c_str());
                Serial.printf("RSSI: %d\n", rssi);
                Serial.println("Reason: Suspicious BLE module near payment terminal");
            }
            s_bleCount++;
        }
    }
};

static ScanCallbacks s_callbacks;

void ble_scanner_init() {
    BLEDevice::init("RagnarScanner");
    s_pScan = BLEDevice::getScan();
    s_pScan->setAdvertisedDeviceCallbacks(&s_callbacks, false);
    s_pScan->setActiveScan(true);
    s_pScan->setInterval(100);
    s_pScan->setWindow(99);
}

void ble_scanner_start(BleMode mode) {
    if (s_mode != BLE_MODE_OFF) ble_scanner_stop();
    s_mode = mode;
    s_bleCount = 0;
    s_adCounts.clear();
    s_spamWindowStart = millis();
    s_pScan->clearResults();
    s_pScan->start(BLE_SCAN_WINDOW_MS / 1000, false);
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
