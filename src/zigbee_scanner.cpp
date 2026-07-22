#include "zigbee_scanner.h"

#if HUGINN_HAS_ZIGBEE

#include "config.h"
#if HUGINN_HAS_GPS
#include "gps_reader.h"
#endif
#include <set>
#include <string.h>
#include "soc/soc_caps.h"

// The 802.15.4 driver headers only exist on SoCs that actually have the radio
// (ESP32-C5 / C6 / H2). If someone sets -DHUGINN_HAS_ZIGBEE=1 on a board that
// can't do it (S3, classic ESP32), fall back to safe stubs that announce the
// mismatch once instead of failing to link.
#if defined(SOC_IEEE802154_SUPPORTED) && SOC_IEEE802154_SUPPORTED
#include "esp_ieee802154.h"
#define HUGINN_ZB_RADIO 1
#else
#define HUGINN_ZB_RADIO 0
#endif

#if HUGINN_ZB_RADIO

// ---- State ----
// The receive callback runs in the 802.15.4 driver task; the counters are read
// from the scan-cycle task, so the unique-device sets are guarded by a mutex.
static SemaphoreHandle_t s_mutex = nullptr;
static std::set<String>  s_phaseSeen;    // unique devices in the current phase
static std::set<String>  s_sessionSeen;  // unique devices since boot
static volatile bool     s_running  = false;
static volatile uint8_t  s_channel  = ZIGBEE_CHANNEL_MIN;
static bool              s_started  = false;

static const char* frameTypeName(uint8_t ftype) {
    switch (ftype) {
        case 0: return "beacon";
        case 1: return "data";
        case 3: return "cmd";
        default: return "other";
    }
}

// Format an 8-byte 802.15.4 extended address (little-endian on the wire) as a
// canonical MSB-first EUI-64 hex string, e.g. "AABBCCDDEEFF0011".
static String formatExt(const uint8_t* le) {
    char buf[17];
    for (int i = 0; i < 8; ++i) {
        snprintf(&buf[i * 2], 3, "%02X", le[7 - i]);
    }
    buf[16] = '\0';
    return String(buf);
}

static String hex16(uint16_t v) {
    char buf[7];
    snprintf(buf, sizeof(buf), "0x%04X", v);
    return String(buf);
}

// Emit one device, deduplicated to once per phase (keeps chatty mains-powered
// routers from flooding the serial link). The session set drives the tally.
static void emitDevice(const String& key, const String& panidStr,
                       bool haveExt, const String& extStr, uint16_t shortAddr,
                       bool haveShort, uint8_t ftype, int rssi, int lqi) {
    bool isNew = false;
    if (s_mutex && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        if (s_phaseSeen.find(key) == s_phaseSeen.end()) {
            s_phaseSeen.insert(key);
            isNew = true;
        }
        if (s_sessionSeen.size() < ZIGBEE_SESSION_TRACK_CAP) {
            s_sessionSeen.insert(key);
        }
        xSemaphoreGive(s_mutex);
    }
    if (!isNew) return;

    // Build the JSON line. `addr` (EUI-64) is preferred; short-only devices
    // carry `short` instead so the host can still identify them.
    String line = "{\"type\":\"ZIGBEE\",\"panid\":\"" + panidStr + "\"";
    if (haveExt) {
        line += ",\"addr\":\"" + extStr + "\"";
    }
    if (haveShort) {
        line += ",\"short\":\"" + hex16(shortAddr) + "\"";
    }
    line += ",\"channel\":" + String((int)s_channel);
    line += ",\"rssi\":" + String(rssi);
    line += ",\"lqi\":" + String(lqi);
    line += ",\"ftype\":\"" + String(frameTypeName(ftype)) + "\"";
#if HUGINN_HAS_GPS
    GpsPosition gp = gps_get_position();
    if (gp.fix) {
        char pos[96];
        snprintf(pos, sizeof(pos),
                 ",\"lat\":%.7f,\"lon\":%.7f,\"speed_kph\":%.2f",
                 gp.lat, gp.lon, gp.speed_kph);
        line += pos;
    }
#endif
    line += "}";
    Serial.println(line);
}

// ---- 802.15.4 MAC header parser ----
// Extracts the source PAN ID and source address from a raw frame so we can key
// a device by identity. Only legacy frame versions (2003 / 2006) are parsed —
// 2015 frames use different PAN-ID-presence rules and are skipped rather than
// risk misreading the addressing fields.
static void parseFrame(const uint8_t* frame, int rssi, int lqi) {
    const uint8_t len   = frame[0];        // PSDU length, includes 2-byte FCS
    const uint8_t* psdu = frame + 1;
    if (len < 5) return;                   // FCF(2) + seq(1) + FCS(2) minimum

    const uint16_t fcf   = (uint16_t)psdu[0] | ((uint16_t)psdu[1] << 8);
    const uint8_t ftype  = fcf & 0x07;
    const bool panComp   = (fcf >> 6) & 0x01;
    const uint8_t destMd = (fcf >> 10) & 0x03;
    const uint8_t ver    = (fcf >> 12) & 0x03;
    const uint8_t srcMd  = (fcf >> 14) & 0x03;

    if (ftype == 2) return;                // ACK carries no addresses
    if (ver >= 2)   return;                // skip 2015 frames (different rules)
    if (srcMd == 0) return;                // no source address → nothing to key

    size_t off = 3;                        // past FCF + sequence number
    const size_t maxOff = (len >= 2) ? (size_t)(len - 2) : 0; // exclude FCS

    uint16_t destPan = 0;
    bool haveDestPan = false;

    // ---- Destination addressing ----
    if (destMd != 0) {
        if (off + 2 > maxOff) return;
        destPan = (uint16_t)psdu[off] | ((uint16_t)psdu[off + 1] << 8);
        off += 2;
        haveDestPan = true;
        if (destMd == 2) {                 // short (16-bit)
            if (off + 2 > maxOff) return;
            off += 2;
        } else if (destMd == 3) {          // extended (64-bit)
            if (off + 8 > maxOff) return;
            off += 8;
        } else {
            return;                        // reserved addressing mode
        }
    }

    // ---- Source addressing ----
    uint16_t srcPan = 0;
    if (panComp && haveDestPan) {
        srcPan = destPan;                  // PAN ID compression: reuse dest PAN
    } else {
        if (off + 2 > maxOff) return;
        srcPan = (uint16_t)psdu[off] | ((uint16_t)psdu[off + 1] << 8);
        off += 2;
    }

    bool haveExt = false, haveShort = false;
    String extStr;
    uint16_t shortAddr = 0;

    if (srcMd == 2) {                      // short source
        if (off + 2 > maxOff) return;
        shortAddr = (uint16_t)psdu[off] | ((uint16_t)psdu[off + 1] << 8);
        haveShort = true;
    } else if (srcMd == 3) {               // extended source
        if (off + 8 > maxOff) return;
        extStr = formatExt(&psdu[off]);
        haveExt = true;
    } else {
        return;                            // reserved
    }

    // Beacons carry the coordinator's PAN in the *source* PAN field; if a
    // frame only had a dest PAN, fall back to that so we still tag a network.
    uint16_t panId = (panComp && haveDestPan) ? destPan : srcPan;
    String panidStr = hex16(panId);

    // Identity key: extended address is globally unique; a short address is
    // only unique within its PAN, so scope it with the PAN ID.
    String key = haveExt ? extStr : (panidStr + ":" + hex16(shortAddr));

    emitDevice(key, panidStr, haveExt, extStr, shortAddr, haveShort,
               ftype, rssi, lqi);
}

// ---- 802.15.4 driver receive callback ----
// Runs in the driver's task context. Keep it short; re-arm receive so the next
// frame is delivered.
extern "C" void esp_ieee802154_receive_done(uint8_t* frame,
                                            esp_ieee802154_frame_info_t* frame_info) {
    if (s_running && frame && frame[0] >= 5) {
        int rssi = frame_info ? (int)frame_info->rssi : -100;
        int lqi  = frame_info ? (int)frame_info->lqi  : 0;
        parseFrame(frame, rssi, lqi);
    }
    esp_ieee802154_receive();
}

void zigbee_scanner_init() {
    if (s_started) return;
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();

    esp_err_t err = esp_ieee802154_enable();
    if (err != ESP_OK) {
        Serial.printf("[ZIGBEE] esp_ieee802154_enable failed: %d\n", (int)err);
        return;
    }
    esp_ieee802154_set_promiscuous(true);
    esp_ieee802154_set_rx_when_idle(true);
    esp_ieee802154_set_channel(s_channel);
    s_started = true;
    Serial.println("[ZIGBEE] 802.15.4 radio ready (promiscuous)");
}

void zigbee_scanner_start() {
    if (!s_started) zigbee_scanner_init();
    if (!s_started) return;
    if (s_mutex && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        s_phaseSeen.clear();
        xSemaphoreGive(s_mutex);
    }
    s_running = true;
    esp_ieee802154_set_channel(s_channel);
    esp_ieee802154_receive();
}

void zigbee_scanner_stop() {
    s_running = false;
    // Leave the radio enabled but idle; re-armed on the next start(). The
    // driver stops delivering once we stop calling receive() from the callback
    // while s_running is false.
}

void zigbee_scanner_set_channel(uint8_t channel) {
    if (channel < ZIGBEE_CHANNEL_MIN || channel > ZIGBEE_CHANNEL_MAX) return;
    s_channel = channel;
    if (s_started && s_running) {
        esp_ieee802154_set_channel(channel);
        esp_ieee802154_receive();
    }
}

int zigbee_scanner_count() {
    int n = 0;
    if (s_mutex && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        n = (int)s_phaseSeen.size();
        xSemaphoreGive(s_mutex);
    }
    return n;
}

int zigbee_scanner_session_count() {
    int n = 0;
    if (s_mutex && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        n = (int)s_sessionSeen.size();
        xSemaphoreGive(s_mutex);
    }
    return n;
}

#else  // HUGINN_ZB_RADIO == 0 : flag set but this SoC has no 802.15.4 radio

void zigbee_scanner_init() {
    static bool warned = false;
    if (!warned) {
        Serial.println("[ZIGBEE] HUGINN_HAS_ZIGBEE set but this SoC has no "
                       "802.15.4 radio — Zigbee scan disabled");
        warned = true;
    }
}
void zigbee_scanner_start() {}
void zigbee_scanner_stop() {}
void zigbee_scanner_set_channel(uint8_t) {}
int  zigbee_scanner_count() { return 0; }
int  zigbee_scanner_session_count() { return 0; }

#endif // HUGINN_ZB_RADIO

#endif // HUGINN_HAS_ZIGBEE
