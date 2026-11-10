#include "ble_scanner.h"
#include "display_manager.h"
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

// ---- Verified advertisement fields ----
// The Arduino BLE library's getName()/getManufacturerData()/service-UUID
// getters cannot be trusted on the NimBLE back-end: its parseAdvertisement()
// reads the first length byte BEFORE checking the payload length, so a
// zero-length scan response (which most Apple devices send) makes it parse
// whatever the controller left in the event buffer — i.e. the PREVIOUS
// device's advertisement. Names, service UUIDs and manufacturer data then get
// pinned on the wrong MAC (seen live: one Find My beacon "detected" again
// under 2-3 unrelated Apple MACs within seconds).
//
// The library does keep a correctly-bounded copy of each device's own bytes
// (getPayload(), ADV + scan responses appended), so every detection below is
// driven from a fresh, bounds-checked walk of that buffer instead.
struct AdvFields {
    String  name;                    // Complete (0x09) or shortened (0x08) local name
    bool    flipperUuid   = false;   // advertises the Flipper Zero serial service
    int8_t  flipperColor  = -1;      // from the 16-bit colour UUID: 0 White, 1 Black, 2 Transparent
    bool    findMy        = false;   // Apple Find My "separated" (offline-finding) advert
    uint8_t findMyStatus  = 0;       // its status byte
    uint8_t findMyRaw[29] = {0};     // company id + payload, for the audit line
};

// FLIPPER_SERVICE_UUID in over-the-air (little-endian) byte order.
static const uint8_t kFlipperUuidLE[16] = {
    0x50, 0xea, 0xda, 0x30, 0x88, 0x83, 0xb8, 0x9f,
    0x60, 0x4f, 0x15, 0xf3, 0x01, 0x00, 0x40, 0x8e
};

static void parseAdvFields(BLEAdvertisedDevice& dev, AdvFields& f) {
    const uint8_t* p   = dev.getPayload();
    const size_t   len = dev.getPayloadLength();
    if (!p) return;
    bool completeName = false;
    size_t i = 0;
    while (i < len) {
        const uint8_t l = p[i];
        if (l == 0) { i++; continue; }          // padding between appended payloads
        if (i + 1 + (size_t)l > len) break;     // truncated structure — stop, never over-read
        const uint8_t  type = p[i + 1];
        const uint8_t* d    = p + i + 2;
        const size_t   dl   = (size_t)l - 1;
        switch (type) {
            case 0x09:                          // Complete Local Name
            case 0x08:                          // Shortened Local Name
                if (type == 0x09 || !completeName) {
                    char buf[32];
                    size_t n = dl < sizeof(buf) - 1 ? dl : sizeof(buf) - 1;
                    memcpy(buf, d, n);
                    buf[n] = '\0';
                    f.name = buf;
                    if (type == 0x09) completeName = true;
                }
                break;
            case 0x02:                          // 16-bit service UUIDs (incomplete)
            case 0x03:                          // 16-bit service UUIDs (complete)
                // Stock Flipper firmware advertises its shell colour as a
                // 16-bit UUID: 0x3081 Black, 0x3082 White, 0x3083 Transparent.
                for (size_t o = 0; o + 2 <= dl; o += 2) {
                    const uint16_t u = (uint16_t)d[o] | ((uint16_t)d[o + 1] << 8);
                    if (u == 0x3081)      f.flipperColor = 1;
                    else if (u == 0x3082) f.flipperColor = 0;
                    else if (u == 0x3083) f.flipperColor = 2;
                }
                break;
            case 0x06:                          // 128-bit service UUIDs (incomplete)
            case 0x07:                          // 128-bit service UUIDs (complete)
                for (size_t o = 0; o + 16 <= dl; o += 16)
                    if (memcmp(d + o, kFlipperUuidLE, 16) == 0) f.flipperUuid = true;
                break;
            case 0xFF:                          // Manufacturer Specific Data
                // Apple Find My offline-finding ("separated") advert is exactly
                // 4C 00 | 12 19 | status | 22 key bytes | key bits | hint.
                if (dl == 29 && d[0] == (APPLE_COMPANY_ID & 0xFF) && d[1] == (APPLE_COMPANY_ID >> 8)
                        && d[2] == 0x12 && d[3] == 0x19) {
                    f.findMy       = true;
                    f.findMyStatus = d[4];
                    memcpy(f.findMyRaw, d, sizeof(f.findMyRaw));
                }
                break;
            default:
                break;
        }
        i += 1 + (size_t)l;
    }
}

// ---- Flipper Zero detection heuristics ----
// Any one of these marks a Flipper (same signals as Wall of Flippers):
//   * the 16-bit colour service UUID (0x3081-0x3083) — also gives the colour
//   * the 128-bit Flipper serial service UUID
//   * a name starting with "Flipper"
//   * a MAC in a Flipper block: 0C:FA:22 (Flipper Devices Inc.) or
//     80:E1:26 / 80:E1:27 (the STMicro block of the STM32WB radio in units
//     built before Flipper had its own prefix). The owner can rename the
//     device and custom firmware can drop the UUIDs, but not hide a public MAC.
//     The two STMicro blocks are not exclusive to Flipper, so another STM32WB
//     product could match — rare, but it is the weakest of the four signals.
static bool isFlipperMac(const String& mac) {
    return mac.startsWith("80:e1:26") || mac.startsWith("80:e1:27") || mac.startsWith("0c:fa:22");
}

static bool isFlipperDevice(const AdvFields& f, const String& mac, uint8_t& color) {
    if (f.flipperColor < 0 && !f.flipperUuid && !f.name.startsWith("Flipper") && !isFlipperMac(mac))
        return false;
    color = 0;
    if (f.flipperColor >= 0) {
        color = (uint8_t)f.flipperColor;
    } else if (f.name.indexOf("Flipper") >= 0) {
        if (f.name.indexOf("Black") >= 0)      color = 1;
        else if (f.name.indexOf("Trans") >= 0) color = 2;
    }
    return true;
}

// ---- AirTag / Find My tracker detection ----
// Apple's manufacturer data is [company 0x004C][type][len][payload...].
//   * 0x07 is AirPods proximity-pairing — a headset, not a tracker.
//   * 0x12 with length 0x02 is the short "nearby" beacon every Apple device
//     relays while its owner is around — not a separated tracker.
//   * 0x12 with length 0x19 is the full offline-finding payload broadcast by
//     a device separated from its owner. But that includes every iPhone, iPad
//     and Mac that is merely offline or asleep, so the status byte's device
//     class (bits 4-5, same decoding as the AirGuard tracker detector) is
//     checked too:
//         0 = Apple device (phone / tablet / Mac)  -> NOT a tracker
//         1 = AirTag, 2 = third-party Find My tag, 3 = AirPods  -> tracker
// Trackers rotate their MAC, so one physical tag legitimately reappears
// under a fresh anonymous identity.
static uint8_t findMyClass(const AdvFields& f) { return (f.findMyStatus >> 4) & 0x03; }

static bool isAirTagDevice(const AdvFields& f) {
    return f.findMy && findMyClass(f) != 0;
}

static const char* findMyClassName(uint8_t cls) {
    switch (cls) {
        case 1:  return "airtag";
        case 2:  return "findmy-tag";
        case 3:  return "airpods";
        default: return "apple-device";
    }
}

// ---- Skimmer detection ----
static bool isSkimmerDevice(const AdvFields& f) {
    return isSkimmerName(f.name);
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
        int rssi    = advertisedDevice.getRSSI();

        // Re-parse this device's own payload (see AdvFields) rather than trust
        // the library getters, which can carry another device's data.
        AdvFields adv;
        parseAdvFields(advertisedDevice, adv);
        const String& name = adv.name;

        // Always check for spam
        checkSpam(mac);

        uint8_t flipperColor = 0;
        bool flipper = isFlipperDevice(adv, mac, flipperColor);
        bool airtag  = isAirTagDevice(adv);
        bool skimmer = isSkimmerDevice(adv);

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

        // One audit line per newly flagged tracker: the exact bytes that
        // tripped the heuristic, so any hit can be checked after the fact.
        if (airtag && airtagNew) {
            char hex[2 * sizeof(adv.findMyRaw) + 1];
            for (size_t k = 0; k < sizeof(adv.findMyRaw); k++)
                snprintf(hex + 2 * k, 3, "%02x", adv.findMyRaw[k]);
            Serial.printf("[TRACKER] mac=%s kind=%s status=0x%02x rssi=%d mfr=%s\n",
                          mac.c_str(), findMyClassName(findMyClass(adv)),
                          adv.findMyStatus, rssi, hex);
        }

        // Proximity alert LED — keep it lit while the device is seen, in ANY
        // BLE mode. Detection runs regardless of mode; only the serial output
        // below is mode-gated, so notifying here (not inside the mode blocks)
        // means the LED doesn't go dark when the scan cycle rotates to a mode
        // that wouldn't print this device. Skimmer takes priority over Flipper.
        // Feed the on-device UI (no-op on boards without the animated display).
        display_note_ble(mac.c_str(), rssi, flipper, airtag, skimmer);

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
