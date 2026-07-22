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
                       bool haveShort, uint8_t ftype, int rssi, int lqi,
                       const char* proto) {
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
    line += ",\"proto\":\"" + String(proto) + "\"";
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

// ---- Protocol classification ----
// Zigbee and Thread both ride on 802.15.4; they can be told apart from what
// sits just above the MAC header. Best-effort: link-layer-encrypted data frames
// are opaque, so those fall back to the generic "802.15.4". (Matter-over-Thread
// is ordinary Thread traffic at this layer, so it reports as "thread".)
static const char* classifyProto(const uint8_t* psdu, size_t maxOff,
                                 size_t payloadOff, uint8_t ftype,
                                 bool secEnabled) {
    if (ftype == 0) {  // beacon — payload starts with the stack's protocol ID
        // MAC beacon body = Superframe(2) + GTS + Pending-Address, then the
        // beacon payload whose first byte is the protocol ID (Zigbee = 0x00,
        // Thread = 0x03). Walk the intermediate fields to reach it.
        size_t o = payloadOff;
        if (o + 2 > maxOff) return "802.15.4";
        o += 2;                                       // superframe specification
        if (o + 1 > maxOff) return "802.15.4";
        uint8_t gts = psdu[o++];                      // GTS specification
        uint8_t gtsCount = gts & 0x07;
        if (gtsCount) o += 1 + (size_t)gtsCount * 3;  // directions + descriptors
        if (o + 1 > maxOff) return "802.15.4";
        uint8_t pend = psdu[o++];                     // Pending-Address spec
        o += (size_t)(pend & 0x07) * 2 + (size_t)((pend >> 4) & 0x07) * 8;
        if (o >= maxOff) return "802.15.4";
        uint8_t pid = psdu[o];
        if (pid == 0x00) return "zigbee";
        if (pid == 0x03) return "thread";
        return "802.15.4";
    }
    // Data / command frames. An encrypted frame's first payload byte is the
    // security-control header, not the network dispatch, so don't guess.
    if (secEnabled) return "802.15.4";
    if (payloadOff >= maxOff) return "802.15.4";
    uint8_t b = psdu[payloadOff];
    // 6LoWPAN dispatch → Thread (Thread carries IPv6 over 6LoWPAN).
    if (b == 0x41 || b == 0x42 || b == 0x50) return "thread";       // IPv6/HC1/BC0
    if ((b & 0xE0) == 0x60) return "thread";                        // IPHC  011xxxxx
    if ((b & 0xC0) == 0x80) return "thread";                        // MESH  10xxxxxx
    if ((b & 0xF8) == 0xC0 || (b & 0xF8) == 0xE0) return "thread";  // FRAG1/FRAGN
    // Zigbee NWK frame control: protocol version 2 (Zigbee PRO) in bits 2–5.
    if ((b & 0x3C) == 0x08) return "zigbee";
    return "802.15.4";
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
        off += 2;                          // advance to the MAC payload
    } else if (srcMd == 3) {               // extended source
        if (off + 8 > maxOff) return;
        extStr = formatExt(&psdu[off]);
        haveExt = true;
        off += 8;                          // advance to the MAC payload
    } else {
        return;                            // reserved
    }

    // Distinguish Zigbee vs Thread from the bytes above the MAC header. `off`
    // now points at the MAC payload (or the auxiliary security header, handled
    // inside classifyProto). The security-enabled bit is FCF bit 3.
    const bool secEnabled = (fcf >> 3) & 0x01;
    const char* proto = classifyProto(psdu, maxOff, off, ftype, secEnabled);

    // Beacons carry the coordinator's PAN in the *source* PAN field; if a
    // frame only had a dest PAN, fall back to that so we still tag a network.
    uint16_t panId = (panComp && haveDestPan) ? destPan : srcPan;
    String panidStr = hex16(panId);

    // Identity key: extended address is globally unique; a short address is
    // only unique within its PAN, so scope it with the PAN ID.
    String key = haveExt ? extStr : (panidStr + ":" + hex16(shortAddr));

    emitDevice(key, panidStr, haveExt, extStr, shortAddr, haveShort,
               ftype, rssi, lqi, proto);
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
    // Announced once; the radio is now enabled/disabled around every wardrive
    // Zigbee phase (see zigbee_scanner_stop), so don't reprint each cycle.
    static bool announced = false;
    if (!announced) {
        Serial.println("[ZIGBEE] 802.15.4 radio ready (promiscuous)");
        announced = true;
    }
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
    // Release the shared 2.4 GHz radio. On the ESP32-C5, 802.15.4 shares one
    // 2.4 GHz radio with WiFi and BLE. Once a sweep put the 802.15.4 radio into
    // receive (rx_when_idle=true keeps it listening), it stayed there — starving
    // the WiFi scanner, so every wardrive WiFi phase AFTER the first Zigbee phase
    // returned zero networks. Fully disable the radio here so WiFi reclaims it;
    // the next start() re-enables it (s_started cleared → zigbee_scanner_init
    // runs again).
    if (s_started) {
        esp_ieee802154_disable();
        s_started = false;
    }
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
