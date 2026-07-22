#include "ble_scanner.h"
#include "config.h"
#include "runtime_config.h"
#include "skimmer_led.h"
#if HUGINN_HAS_GPS
#include "gps_reader.h"
#endif
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <map>
#include <set>

static BleMode s_mode = BLE_MODE_OFF;
static int s_bleCount = 0;
static int s_flipperCount = 0;
static int s_airtagCount = 0;
static int s_skimmerCount = 0;
static BLEScan* s_pScan = nullptr;

// Spam detection: MAC -> add count within window
static std::map<String, uint32_t> s_adCounts;
static unsigned long s_spamWindowStart = 0;

// ---- Session totals — unique MACs observed since boot ----
// Touched by the BLE callback (BLE task) and by the display task on the
// other core, so all access goes through s_sessionMutex.
#define BLE_SESSION_TRACK_CAP 4096
static SemaphoreHandle_t s_sessionMutex = nullptr;
static std::set<String> s_sessionBleMacs;
static std::set<String> s_sessionFlipperMacs;
static std::set<String> s_sessionAirtagMacs;
static std::set<String> s_sessionSkimmerMacs;

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

// ---- AirTag / Find My tracker detection ----
// Apple's manufacturer data is [company 0x004C][type][len][payload...]. The
// old heuristic matched type 0x12 OR 0x07 with no length check, which flooded
// with false positives:
//   * 0x07 is AirPods proximity-pairing — a headset, not a tracker.
//   * 0x12 (Find My / Offline-Finding) is relayed by EVERY nearby Apple device
//     (iPhone, Mac, AirPods…) as a short 2-byte "nearby" beacon, not just tags.
// A standalone tracker separated from its owner broadcasts the full
// offline-finding payload: type 0x12 with length byte 0x19 (25 bytes of status
// + rotating public key). Requiring that keeps the strong tracker signal and
// drops the ambient Apple-device chatter. AirTags rotate their MAC (~15 min),
// so a physical tag legitimately reappears under a fresh anonymous identity.
static bool isAirTagDevice(BLEAdvertisedDevice& dev) {
    if (!dev.haveManufacturerData()) return false;
    String mfr = dev.getManufacturerData();
    if (mfr.length() < 4) return false;

    uint16_t companyId = (uint16_t)(uint8_t)mfr[0] | ((uint16_t)(uint8_t)mfr[1] << 8);
    if (companyId != APPLE_COMPANY_ID) return false;

    const uint8_t type = (uint8_t)mfr[2];
    const uint8_t len  = (uint8_t)mfr[3];
    return type == 0x12 && len == 0x19;   // Find My, full separated-state payload
}

// ---- Skimmer detection ----
static bool isSkimmerDevice(BLEAdvertisedDevice& dev) {
    String name = String(dev.getName().c_str());
    return isSkimmerName(name);
}

// ---- BLE Spam detection ----
static void checkSpam(const String& mac) {
    unsigned long now = millis();
    if (now - s_spamWindowStart > BLE_SPAM_WINDOW_MS) {
        s_adCounts.clear();
        s_spamWindowStart = now;
    }
    s_adCounts[mac]++;
    if (s_adCounts[mac] == g_bleSpamThreshold) {
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

        // Session totals — unique MACs since boot (capped to bound memory).
        // Capture whether this AirTag MAC is newly seen so we alert once per
        // unique tracker instead of on every advertisement (the old static
        // counter recounted the same tag every sweep).
        bool airtagNew    = false;
        int  airtagUnique = 0;
        if (s_sessionMutex && xSemaphoreTake(s_sessionMutex, portMAX_DELAY) == pdTRUE) {
            if (s_sessionBleMacs.size() < BLE_SESSION_TRACK_CAP) s_sessionBleMacs.insert(mac);
            if (flipper) s_sessionFlipperMacs.insert(mac);
            if (airtag) {
                airtagNew    = s_sessionAirtagMacs.insert(mac).second;
                airtagUnique = (int)s_sessionAirtagMacs.size();
            }
            if (skimmer) s_sessionSkimmerMacs.insert(mac);
            xSemaphoreGive(s_sessionMutex);
        }

        // Proximity alert LED — keep it lit while the device is seen, in ANY
        // BLE mode. Detection runs regardless of mode; only the serial output
        // below is mode-gated, so notifying here (not inside the mode blocks)
        // means the LED doesn't go dark when the scan cycle rotates to a mode
        // that wouldn't print this device. Skimmer takes priority over Flipper.
        if (skimmer)      skimmer_led_notify(rssi, SKIMMER_LED_SKIMMER); // red<->white
        else if (flipper) skimmer_led_notify(rssi, SKIMMER_LED_FLIPPER); // blue<->white

        // ---- Filtered mode: only output Flipper / AirTag ----
        if (s_mode == BLE_MODE_FILTERED) {
            if (flipper) {
                s_flipperCount++;
                const char* colorStr = "White";
                if (flipperColor == 1) colorStr = "Black";
                else if (flipperColor == 2) colorStr = "Transparent";

                Serial.printf("Found %s Flipper Device:\n", colorStr);
                Serial.printf("MAC: %s,\n", mac.c_str());
                Serial.printf("Name: %s,\n", name.c_str());
                Serial.printf("RSSI: %d\n", rssi);
            }
            if (airtag && airtagNew) {
                s_airtagCount++;
                Serial.println("AirTag found!");
                Serial.printf("Tag: %d\n", airtagUnique);
                Serial.printf("MAC Address: %s\n", mac.c_str());
                Serial.printf("RSSI: %d\n", rssi);
            }
            s_bleCount++;
            return;
        }

        // ---- Skimmer mode ----
        if (s_mode == BLE_MODE_SKIMMER) {
            if (skimmer) {
                s_skimmerCount++;
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
#if HUGINN_HAS_GPS
            GpsPosition gp = gps_get_position();
            if (gp.fix) {
                Serial.printf("{\"type\":\"BLE\",\"mac\":\"%s\",\"name\":\"%s\",\"rssi\":%d,\"lat\":%.7f,\"lon\":%.7f,\"speed_kph\":%.2f,\"speed_mps\":%.2f}\n",
                              mac.c_str(), name.length() > 0 ? name.c_str() : "", rssi,
                              gp.lat, gp.lon, gp.speed_kph, gp.speed_kph / 3.6f);
            } else {
                Serial.printf("{\"type\":\"BLE\",\"mac\":\"%s\",\"name\":\"%s\",\"rssi\":%d}\n",
                              mac.c_str(), name.length() > 0 ? name.c_str() : "", rssi);
            }
#else
            Serial.printf("{\"type\":\"BLE\",\"mac\":\"%s\",\"name\":\"%s\",\"rssi\":%d}\n",
                          mac.c_str(), name.length() > 0 ? name.c_str() : "", rssi);
#endif
            Serial.flush();

            if (flipper) {
                s_flipperCount++;
                const char* colorStr = "White";
                if (flipperColor == 1) colorStr = "Black";
                else if (flipperColor == 2) colorStr = "Transparent";
                Serial.printf("Found %s Flipper Device:\n", colorStr);
                Serial.printf("MAC: %s,\n", mac.c_str());
                Serial.printf("Name: %s,\n", name.c_str());
                Serial.printf("RSSI: %d\n", rssi);
            }
            if (airtag && airtagNew) {
                s_airtagCount++;
                Serial.println("AirTag found!");
                Serial.printf("Tag: %d\n", airtagUnique);
                Serial.printf("MAC Address: %s\n", mac.c_str());
                Serial.printf("RSSI: %d\n", rssi);
            }
            if (skimmer) {
                s_skimmerCount++;
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
    if (!s_sessionMutex) s_sessionMutex = xSemaphoreCreateMutex();
    BLEDevice::init("HuginnESP");
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
    s_flipperCount = 0;
    s_airtagCount = 0;
    s_skimmerCount = 0;
    s_adCounts.clear();
    s_spamWindowStart = millis();
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

int ble_scanner_flipper_count() {
    return s_flipperCount;
}

int ble_scanner_airtag_count() {
    return s_airtagCount;
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
int ble_scanner_session_flipper_count() { return sessionSetSize(s_sessionFlipperMacs); }
int ble_scanner_session_airtag_count()  { return sessionSetSize(s_sessionAirtagMacs); }
int ble_scanner_session_skimmer_count() { return sessionSetSize(s_sessionSkimmerMacs); }
