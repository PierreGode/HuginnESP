// =====================================================================
//  Animated 160x80 UI for the LilyGO T-Dongle-C5 — see tdongle_ui.h.
//
//  Portable C++ only (also compiled natively by sim/tdongle_ui_sim.cpp).
//  The ESP32-C5 has no FPU, so the per-pixel paths use integer math and a
//  Q12 sine table; floats are kept to a handful of per-frame easings.
// =====================================================================

#include "tdongle_ui.h"
#include "tdongle_font.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

const int W = TD_UI_W;
const int H = TD_UI_H;

// ---------------------------------------------------------------------
//  Viking palette (mirrors the S3 display / web flasher)
// ---------------------------------------------------------------------
constexpr uint16_t RGB(uint32_t h) {
    return (uint16_t)(((h >> 8) & 0xF800) | ((h >> 5) & 0x07E0) | ((h >> 3) & 0x001F));
}
const uint16_t COL_BG     = RGB(0x050403);
const uint16_t COL_HEADER = RGB(0x1c160d);
const uint16_t COL_BONE   = RGB(0xf2ecdc);
const uint16_t COL_BRONZE = RGB(0xd8a850);
const uint16_t COL_RUNE   = RGB(0xffd98a);
const uint16_t COL_AGED   = RGB(0x8a6a30);
const uint16_t COL_DIM    = RGB(0x3a2c14);
const uint16_t COL_FAINT  = RGB(0x1e170b);
const uint16_t COL_ASH    = RGB(0x7c7466);
const uint16_t COL_BLOOD  = RGB(0xf03030);
const uint16_t COL_STEEL  = RGB(0x60c8ff);
const uint16_t COL_MOSS   = RGB(0x70e0a0);
const uint16_t COL_WHITE  = 0xFFFF;

// ---------------------------------------------------------------------
//  Timing
// ---------------------------------------------------------------------
const uint32_t SPLASH_MS      = 3900;
const uint32_t ALERT_MS       = 4500;
const uint32_t PAGE_ROTATE_MS = 9000;
const uint32_t PAGE_TRANS_MS  = 380;
const uint32_t WIFI_EXPIRE_MS = 30000;
const uint32_t BLE_EXPIRE_MS  = 20000;

enum { PAGE_RADAR, PAGE_SPECTRUM, PAGE_TARGETS, PAGE_HUGINN, PAGE_COUNT };
const char* const PAGE_NAMES[PAGE_COUNT] = { "RADAR", "SPECTRUM", "TARGETS", "HUGINN" };

// ---------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------
struct Dev {
    bool     used;
    uint8_t  channel;   // Wi-Fi channel (0 for BLE)
    uint8_t  sec;       // TdUiSec (Wi-Fi)
    uint8_t  flags;     // TD_BLE_* (BLE)
    int8_t   rssi;
    uint32_t hash;
    uint32_t first;     // ms first seen (this appearance)
    uint32_t last;      // ms last seen
    char     name[17];  // SSID (Wi-Fi) — empty = hidden
};

const int MAX_WIFI = 48;
const int MAX_BLE  = 48;
Dev g_wifi[MAX_WIFI];
Dev g_ble[MAX_BLE];

struct Alert {
    bool     pending;   // raised but not yet started (e.g. during the splash)
    bool     active;
    uint32_t start;
    int      rssi;
    char     type[12];
    char     detail[24];
} g_alert;

// Devices already announced with a full-screen alert (ring buffer).
struct Alerted { uint32_t hash; uint8_t flags; };
const int ALERTED_MAX = 64;
Alerted g_alerted[ALERTED_MAX];
int     g_alertedNext = 0;

const int NUM_CH = 13 + 28;        // 2.4 GHz ch 1-13, then 28 5 GHz channels
float g_barCur[NUM_CH];
float g_barPeak[NUM_CH];

float g_dispWifi = 0, g_dispBle = 0, g_dispThird = 0;

bool     g_started   = false;
uint32_t g_boot      = 0;
int      g_page      = PAGE_RADAR;
int      g_prevPage  = -1;
uint32_t g_pageStart = 0;
bool     g_nextReq   = false;

char     g_ticker[400];
int      g_tickerPx    = 0;
uint32_t g_tickerStart = 0;

int16_t  g_sin[1024];              // Q12
bool     g_sinReady = false;

// Draw target + origin offset (page slide transitions).
uint16_t* g_fb = nullptr;
int g_ox = 0, g_oy = 0;

// ---------------------------------------------------------------------
//  Math helpers
// ---------------------------------------------------------------------
inline int isin(int a) { return g_sin[a & 1023]; }
inline int icos(int a) { return g_sin[(a + 256) & 1023]; }

inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline float easeOut(float t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    float u = 1 - t;
    return 1 - u * u * u;
}

// ---------------------------------------------------------------------
//  Colour helpers (RGB565)
// ---------------------------------------------------------------------
inline uint16_t scale565(uint16_t c, int a) {           // a: 0..256
    if (a <= 0) return 0;
    if (a >= 256) return c;
    int r = ((c >> 11) * a) >> 8;
    int g = (((c >> 5) & 0x3F) * a) >> 8;
    int b = ((c & 0x1F) * a) >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

inline uint16_t mix565(uint16_t c0, uint16_t c1, int t) { // t: 0..256
    t = clampi(t, 0, 256);
    int r = ((c0 >> 11) * (256 - t) + (c1 >> 11) * t) >> 8;
    int g = (((c0 >> 5) & 0x3F) * (256 - t) + ((c1 >> 5) & 0x3F) * t) >> 8;
    int b = ((c0 & 0x1F) * (256 - t) + (c1 & 0x1F) * t) >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

inline uint16_t max565(uint16_t a, uint16_t b) {
    uint16_t r  = (a & 0xF800) > (b & 0xF800) ? (a & 0xF800) : (b & 0xF800);
    uint16_t g  = (a & 0x07E0) > (b & 0x07E0) ? (a & 0x07E0) : (b & 0x07E0);
    uint16_t bl = (a & 0x001F) > (b & 0x001F) ? (a & 0x001F) : (b & 0x001F);
    return (uint16_t)(r | g | bl);
}

inline uint16_t add565(uint16_t a, uint16_t b) {
    int r  = (a >> 11) + (b >> 11);                 if (r > 31) r = 31;
    int g  = ((a >> 5) & 0x3F) + ((b >> 5) & 0x3F); if (g > 63) g = 63;
    int bl = (a & 0x1F) + (b & 0x1F);               if (bl > 31) bl = 31;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

// ---------------------------------------------------------------------
//  Primitives (all clipped, all offset by g_ox/g_oy)
// ---------------------------------------------------------------------
inline void px(int x, int y, uint16_t c) {
    x += g_ox; y += g_oy;
    if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) g_fb[y * W + x] = c;
}
inline void pxMax(int x, int y, uint16_t c) {
    x += g_ox; y += g_oy;
    if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) {
        uint16_t& d = g_fb[y * W + x];
        d = max565(d, c);
    }
}
inline void pxAdd(int x, int y, uint16_t c) {
    x += g_ox; y += g_oy;
    if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) {
        uint16_t& d = g_fb[y * W + x];
        d = add565(d, c);
    }
}

void fillRect(int x, int y, int w, int h, uint16_t c) {
    x += g_ox; y += g_oy;
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    for (int yy = y; yy < y1; yy++) {
        uint16_t* p = g_fb + yy * W;
        for (int xx = x; xx < x1; xx++) p[xx] = c;
    }
}
inline void hline(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
inline void vline(int x, int y, int h, uint16_t c) { fillRect(x, y, 1, h, c); }

void rect(int x, int y, int w, int h, uint16_t c) {
    hline(x, y, w, c);
    hline(x, y + h - 1, w, c);
    vline(x, y, h, c);
    vline(x + w - 1, y, h, c);
}

void circle(int cx, int cy, int r, uint16_t c, bool maxBlend = false) {
    if (r <= 0) { if (maxBlend) pxMax(cx, cy, c); else px(cx, cy, c); return; }
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        const int pts[8][2] = {
            { cx + x, cy + y }, { cx - x, cy + y }, { cx + x, cy - y }, { cx - x, cy - y },
            { cx + y, cy + x }, { cx - y, cy + x }, { cx + y, cy - x }, { cx - y, cy - x } };
        for (int i = 0; i < 8; i++) {
            if (maxBlend) pxMax(pts[i][0], pts[i][1], c);
            else          px(pts[i][0], pts[i][1], c);
        }
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

// 5x7 text, 6 px pitch per scale unit. Returns the x after the last glyph.
int text(int x, int y, const char* s, uint16_t c, int sc = 1) {
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch < 32 || ch > 126) ch = '?';
        int sx = x + g_ox;
        if (sx < W && sx + 6 * sc > 0 && ch != ' ') {
            const uint8_t* g = TD_FONT5X7[ch - 32];
            for (int col = 0; col < 5; col++) {
                uint8_t bits = g[col];
                for (int row = 0; row < 7; row++) {
                    if (!((bits >> row) & 1)) continue;
                    if (sc == 1) px(x + col, y + row, c);
                    else         fillRect(x + col * sc, y + row * sc, sc, sc, c);
                }
            }
        }
        x += 6 * sc;
    }
    return x;
}
inline int textW(const char* s, int sc = 1) { return (int)strlen(s) * 6 * sc; }
inline void textRight(int xr, int y, const char* s, uint16_t c, int sc = 1) {
    text(xr - textW(s, sc) + sc, y, s, c, sc);
}
inline void textCenter(int cx, int y, const char* s, uint16_t c, int sc = 1) {
    text(cx - (textW(s, sc) - sc) / 2, y, s, c, sc);
}

// Shift a few random row bands sideways — cheap "signal glitch".
void glitch(uint32_t seed, int bands, int maxShift) {
    for (int i = 0; i < bands; i++) {
        uint32_t h = hash32(seed * 31 + i);
        int y  = h % H;
        int hh = 1 + ((h >> 8) % 3);
        int sh = (int)((h >> 12) % (2 * maxShift + 1)) - maxShift;
        if (sh == 0) continue;
        for (int yy = y; yy < y + hh && yy < H; yy++) {
            uint16_t* row = g_fb + yy * W;
            if (sh > 0) {
                memmove(row + sh, row, (W - sh) * sizeof(uint16_t));
                for (int x = 0; x < sh; x++) row[x] = 0;
            } else {
                memmove(row, row - sh, (W + sh) * sizeof(uint16_t));
                for (int x = W + sh; x < W; x++) row[x] = 0;
            }
        }
    }
}

// Drifting ember particles (deterministic in time — no per-particle state).
void embers(uint32_t t, int count, int strength) {
    for (int i = 0; i < count; i++) {
        uint32_t h = hash32(0x9E3779B9U * (i + 1));
        int speed = 6 + (h % 14);                         // px / s
        int span  = H + 12;
        int y     = H + 4 - (int)(((uint64_t)t * speed / 1000 + (h >> 8)) % span);
        int sway  = (isin((int)(t / 6) + (int)(h >> 4)) * 5) >> 12;
        int x     = (int)((h >> 16) % W) + sway;
        int life  = clampi((y + 6) * 256 / H, 0, 256);     // fade as it rises
        int flick = 160 + ((isin((int)(t / 2) + (int)(h >> 3)) * 96) >> 12);
        int a     = (life * flick >> 8) * strength >> 8;
        uint16_t c = scale565(mix565(COL_BLOOD, COL_RUNE, life), a);
        pxMax(x, y, c);
        if ((h >> 5) & 1) pxMax(x, y + 1, scale565(c, 110));
    }
}

// ---------------------------------------------------------------------
//  Device tables
// ---------------------------------------------------------------------
Dev* devSlot(Dev* tbl, int n, uint32_t hash, uint32_t now, bool& isNew) {
    Dev* freeSlot = nullptr;
    Dev* oldest   = &tbl[0];
    for (int i = 0; i < n; i++) {
        Dev& d = tbl[i];
        if (d.used && d.hash == hash) { isNew = false; return &d; }
        if (!d.used) { if (!freeSlot) freeSlot = &d; }
        else if ((int32_t)(d.last - oldest->last) < 0 || !oldest->used) oldest = &d;
    }
    isNew = true;
    Dev* d = freeSlot ? freeSlot : oldest;
    memset(d, 0, sizeof(*d));
    d->used  = true;
    d->hash  = hash;
    d->first = now;
    return d;
}

void expire(Dev* tbl, int n, uint32_t now, uint32_t maxAge) {
    for (int i = 0; i < n; i++)
        if (tbl[i].used && (now - tbl[i].last) > maxAge) tbl[i].used = false;
}

int countUsed(const Dev* tbl, int n) {
    int c = 0;
    for (int i = 0; i < n; i++) if (tbl[i].used) c++;
    return c;
}

// Channel -> bar index (0-12 = 2.4 GHz ch 1-13, 13-40 = 5 GHz), -1 if unknown.
int channelIndex(int ch) {
    if (ch >= 1 && ch <= 14) return ch > 13 ? 12 : ch - 1;
    if (ch >= 36 && ch <= 64)   return 13 + (ch - 36) / 4;
    if (ch >= 100 && ch <= 144) return 21 + (ch - 100) / 4;
    if (ch >= 149 && ch <= 177) return 33 + (ch - 149) / 4;
    return -1;
}

// ---------------------------------------------------------------------
//  Header (fixed strip, never slides)
// ---------------------------------------------------------------------
void drawHeader(uint32_t now, const TdUiStatus& st) {
    fillRect(0, 0, W, 9, COL_HEADER);

    // Heartbeat dot.
    int beat = 150 + ((isin((int)(now * 1024 / 900)) * 105) >> 12);
    fillRect(2, 3, 3, 3, scale565(COL_BLOOD, beat));

    // Mode, upper-cased.
    char mode[14];
    const char* m = st.mode ? st.mode : "";
    size_t i = 0;
    for (; m[i] && i < sizeof(mode) - 1; i++) {
        char c = m[i];
        mode[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    mode[i] = 0;
    text(8, 1, mode, COL_BRONZE);

    // Page dots + page name.
    int dx = W - PAGE_COUNT * 5;
    for (int p = 0; p < PAGE_COUNT; p++) {
        if (p == g_page) fillRect(dx + p * 5, 3, 3, 3, COL_RUNE);
        else             px(dx + p * 5 + 1, 4, COL_AGED);
    }
    textRight(dx - 4, 1, PAGE_NAMES[g_page], COL_ASH);

    // Rule with a travelling glint.
    int gx = (int)((now / 9) % 260) - 50;
    for (int x = 0; x < W; x++) {
        int d = x - gx; if (d < 0) d = -d;
        int a = d < 28 ? 256 - d * 9 : 0;
        g_fb[9 * W + x] = mix565(COL_DIM, COL_RUNE, a);
    }
}

// ---------------------------------------------------------------------
//  Page: RADAR
// ---------------------------------------------------------------------
void counterRow(int y, const char* label, float value, uint16_t col, int fillOf) {
    char buf[12];
    int v = (int)(value + 0.5f);
    if (v > 9999) v = 9999;
    snprintf(buf, sizeof(buf), "%d", v);
    text(80, y + 6, label, scale565(col, 170));
    textRight(158, y, buf, col, 2);
    hline(80, y + 17, 79, COL_FAINT);
    int w = clampi((int)(value * 79 / (fillOf > 0 ? fillOf : 1)), 0, 79);
    for (int x = 0; x < w; x++) px(80 + x, y + 17, mix565(scale565(col, 90), col, x * 256 / 79));
}

void drawRadar(uint32_t now, uint32_t /*tPage*/, const TdUiStatus& st) {
    const int cx = 37, cy = 45, R = 33;

    // Range rings + crosshair.
    circle(cx, cy, 11, COL_FAINT);
    circle(cx, cy, 22, COL_FAINT);
    hline(cx - R, cy, 2 * R + 1, COL_FAINT);
    vline(cx, cy - R, 2 * R + 1, COL_FAINT);

    // Sweep: a fan of fading spokes trailing the leading edge.
    const int sweep = (int)((uint64_t)now * 1024 / 2600) & 1023;
    const int FAN = 64;
    for (int k = FAN - 1; k >= 0; k--) {
        int a  = sweep - k * 3;
        int in = (FAN - k) * 256 / FAN;
        uint16_t c = scale565(k == 0 ? COL_RUNE : COL_BRONZE, k == 0 ? 256 : (in * in >> 8) * 150 >> 8);
        int cs = icos(a), sn = isin(a);
        for (int r = 2; r <= R; r++) pxMax(cx + ((cs * r) >> 12), cy + ((sn * r) >> 12), c);
    }

    // Outer ring with compass ticks.
    circle(cx, cy, R, COL_AGED);
    for (int a = 0; a < 1024; a += 128) {
        int cs = icos(a), sn = isin(a);
        px(cx + ((cs * (R - 2)) >> 12), cy + ((sn * (R - 2)) >> 12), COL_BRONZE);
        px(cx + ((cs * (R - 1)) >> 12), cy + ((sn * (R - 1)) >> 12), COL_BRONZE);
    }

    // Blips — angle from the device hash, range from RSSI.
    for (int pass = 0; pass < 2; pass++) {
        const Dev* tbl = pass == 0 ? g_wifi : g_ble;
        const int  n   = pass == 0 ? MAX_WIFI : MAX_BLE;
        for (int i = 0; i < n; i++) {
            const Dev& d = tbl[i];
            if (!d.used) continue;
            int ang = (int)(hash32(d.hash) & 1023);
            int rr  = clampi((-30 - d.rssi) * (R - 5) / 62 + 4, 4, R - 3);
            int x   = cx + ((icos(ang) * rr) >> 12);
            int y   = cy + ((isin(ang) * rr) >> 12);

            bool threat = pass == 1 && d.flags != 0;
            uint16_t col = threat ? COL_BLOOD : (pass == 0 ? COL_RUNE : COL_STEEL);

            // Brightness decays with the angle the sweep has travelled past it.
            int behind = (sweep - ang) & 1023;
            int b = 256 - behind * 256 / 1024;
            b = 120 + (b * b >> 8) * 136 / 256;
            if (threat) b = ((now / 180) & 1) ? 256 : 120;

            pxMax(x, y, scale565(col, b));
            {
                uint16_t g = scale565(col, threat ? 200 : (b - 90) * 3 / 4);
                pxMax(x + 1, y, g); pxMax(x - 1, y, g);
                pxMax(x, y + 1, g); pxMax(x, y - 1, g);
            }
            // Ping ring on first contact.
            uint32_t age = now - d.first;
            if (age < 900) {
                int pr = 1 + (int)(age * 8 / 900);
                circle(x, y, pr, scale565(col, 256 - (int)(age * 256 / 900)), true);
            }
        }
    }
    fillRect(cx - 1, cy - 1, 3, 3, COL_BONE);

    // Counters.
    int omens = st.flippers + st.airtags + st.skimmers;
    bool zig  = st.mode && strcmp(st.mode, "zigbee") == 0;
    counterRow(13, "WIFI", g_dispWifi, COL_RUNE, MAX_WIFI);
    counterRow(36, "BLE", g_dispBle, COL_STEEL, MAX_BLE);
    if (zig) counterRow(59, "ZIG", g_dispThird, COL_MOSS, 32);
    else     counterRow(59, "OMEN", g_dispThird, omens > 0 ? COL_BLOOD : COL_ASH, 8);
}

// ---------------------------------------------------------------------
//  Page: SPECTRUM
// ---------------------------------------------------------------------
void buildTicker() {
    g_ticker[0] = 0;
    size_t len = 0;
    for (int i = 0; i < MAX_WIFI; i++) {
        const Dev& d = g_wifi[i];
        if (!d.used) continue;
        char item[48];
        snprintf(item, sizeof(item), "%s %d ch%d  *  ",
                 d.name[0] ? d.name : "<hidden>", (int)d.rssi, (int)d.channel);
        size_t il = strlen(item);
        if (len + il >= sizeof(g_ticker)) break;
        memcpy(g_ticker + len, item, il + 1);
        len += il;
    }
    if (len == 0) {
        snprintf(g_ticker, sizeof(g_ticker), "listening for wifi...  *  ");
        len = strlen(g_ticker);
    }
    g_tickerPx = (int)len * 6;
}

void drawSpectrum(uint32_t now, uint32_t /*tPage*/, const TdUiStatus&) {
    const int base = 60, maxH = 46;

    // Faint horizontal grid.
    for (int gy = base - 15; gy > base - maxH; gy -= 15)
        for (int x = 2; x < W - 1; x += 3) px(x, gy, COL_FAINT);

    for (int i = 0; i < NUM_CH; i++) {
        bool g5 = i >= 13;
        int x   = g5 ? 74 + (i - 13) * 3 : 3 + i * 5;
        int bw  = g5 ? 2 : 4;
        uint16_t top = g5 ? COL_STEEL : COL_RUNE;
        uint16_t low = g5 ? RGB(0x14506e) : COL_AGED;

        // Idle "noise floor" flicker so empty channels still breathe.
        int noise = 1 + (int)(hash32((now / 110) * 131 + i) % 3);
        int h = (int)(g_barCur[i] + 0.5f);
        if (h < noise) h = noise;

        for (int k = 0; k < h; k++) {
            uint16_t c = mix565(low, top, k * 256 / maxH + 40);
            if (k == h - 1 && h > 3) c = mix565(c, COL_WHITE, 120);
            hline(x, base - k, bw, c);
        }
        int pk = (int)(g_barPeak[i] + 0.5f);
        if (pk > h + 1) hline(x, base - pk, bw, COL_BONE);
    }
    hline(2, base + 1, W - 4, COL_DIM);

    // Scan cursor gliding across the band.
    int sx = 2 + (int)((now / 14) % (W - 4));
    for (int y = base - maxH; y <= base; y++) pxAdd(sx, y, RGB(0x282010));

    text(3, 63, "2.4G", COL_AGED);
    text(74, 63, "5G", RGB(0x2a7ea8));
    char buf[16];
    snprintf(buf, sizeof(buf), "%d AP", countUsed(g_wifi, MAX_WIFI));
    textRight(158, 63, buf, COL_ASH);

    // SSID ticker.
    uint32_t tt = now - g_tickerStart;
    int off = (int)((uint64_t)tt * 34 / 1000);
    if (g_tickerPx == 0 || off >= g_tickerPx) {
        buildTicker();
        g_tickerStart = now;
        off = 0;
    }
    int x = -off;
    while (x < W) { text(x, 72, g_ticker, COL_BONE); x += g_tickerPx; }
    // Fade the ticker edges.
    for (int e = 0; e < 10; e++) {
        int a = e * 26;
        for (int y = 72; y < 79; y++) {
            int xl = e + g_ox, xr = W - 1 - e + g_ox;
            if ((unsigned)xl < (unsigned)W) g_fb[y * W + xl] = scale565(g_fb[y * W + xl], a);
            if ((unsigned)xr < (unsigned)W) g_fb[y * W + xr] = scale565(g_fb[y * W + xr], a);
        }
    }
}

// ---------------------------------------------------------------------
//  Page: TARGETS (strongest signals)
// ---------------------------------------------------------------------
void drawTargets(uint32_t now, uint32_t tPage, const TdUiStatus&) {
    const int ROWS = 5;
    const Dev* top[ROWS];
    int n = 0;
    for (int i = 0; i < MAX_WIFI; i++) {
        const Dev* d = &g_wifi[i];
        if (!d->used) continue;
        int pos = n;
        while (pos > 0 && (top[pos - 1]->rssi < d->rssi ||
               (top[pos - 1]->rssi == d->rssi && top[pos - 1]->hash < d->hash))) pos--;
        if (pos >= ROWS) continue;
        int last = n < ROWS ? n : ROWS - 1;
        for (int k = last; k > pos; k--) top[k] = top[k - 1];
        top[pos] = d;
        if (n < ROWS) n++;
    }

    if (n == 0) {
        int dots = (int)(now / 400) % 4;
        char buf[20];
        snprintf(buf, sizeof(buf), "SEEKING%.*s", dots, "...");
        text(50, 40, buf, COL_AGED);
        return;
    }

    for (int i = 0; i < n; i++) {
        const Dev& d = *top[i];
        int y = 12 + i * 14;

        // Staggered slide-in when the page opens.
        float e = easeOut(((float)tPage - i * 70.0f) / 320.0f);
        int slide = (int)((1.0f - e) * W);
        int ox = g_ox;
        g_ox += slide;

        char name[16];
        snprintf(name, sizeof(name), "%.15s", d.name[0] ? d.name : "<hidden>");
        bool open = d.sec == TD_SEC_OPEN;
        text(2, y, name, open ? COL_BLOOD : COL_BONE);

        char buf[12];
        snprintf(buf, sizeof(buf), "c%d", (int)d.channel);
        textRight(132, y, buf, d.channel > 14 ? RGB(0x2a7ea8) : COL_AGED);
        snprintf(buf, sizeof(buf), "%d", (int)d.rssi);
        textRight(158, y, buf, COL_RUNE);

        // Signal bar with a moving glint.
        int full = 156;
        int w = clampi((d.rssi + 98) * full / 68, 3, full);
        w = (int)(w * e);
        hline(2, y + 9, full, COL_FAINT);
        int gx = (int)((now / 7 + i * 37) % 220) - 30;
        for (int x = 0; x < w; x++) {
            uint16_t c = mix565(COL_AGED, open ? COL_BLOOD : COL_RUNE, x * 256 / full);
            int dg = x - gx; if (dg < 0) dg = -dg;
            if (dg < 10) c = mix565(c, COL_WHITE, (10 - dg) * 20);
            px(2 + x, y + 9, c);
            px(2 + x, y + 10, scale565(c, 120));
        }
        g_ox = ox;
    }
}

// ---------------------------------------------------------------------
//  Page: HUGINN (raven eye + session tally)
// ---------------------------------------------------------------------
void drawEye(uint32_t now, int cx, int cy) {
    const int HW = 26, HH = 15;

    // Blink: open most of the time, a quick shut/open every ~4.3 s.
    uint32_t ph = now % 4300;
    int open = 256;
    if (ph > 4040) {
        int k = (int)(ph - 4040);                 // 0..260
        open = k < 130 ? 256 - k * 256 / 130 : (k - 130) * 256 / 130;
    }

    // Pupil wanders slowly.
    int pxo = (isin((int)(now * 1024 / 5200)) * 9) >> 12;
    int pyo = (isin((int)(now * 1024 / 3700) + 200) * 2) >> 12;

    // Rotating rune ring.
    int rot = (int)(now * 1024 / 9000);
    for (int k = 0; k < 40; k++) {
        int a = rot + k * 1024 / 40;
        int x = cx + ((icos(a) * 30) >> 12), y = cy + ((isin(a) * 30) >> 12);
        px(x, y, (k % 5) == 0 ? COL_RUNE : COL_AGED);
    }

    for (int x = -HW; x <= HW; x++) {
        int h = HH * (HW * HW - x * x) / (HW * HW);
        h = h * open >> 8;
        for (int y = -h; y <= h; y++) {
            int dx = x - pxo, dy = y - pyo;
            int d2 = dx * dx + dy * dy;
            uint16_t c;
            if (d2 <= 16)       c = 0x0000;                                   // pupil
            else if (d2 <= 110) c = mix565(COL_RUNE, RGB(0xa04808), (d2 - 16) * 256 / 94); // iris
            else if (d2 <= 132) c = RGB(0x3a1804);                             // limbus
            else                c = mix565(RGB(0x30200c), RGB(0x0c0804), (x * x) * 256 / (HW * HW));
            px(cx + x, cy + y, c);
        }
        // Lids.
        px(cx + x, cy - h - 1, COL_BRONZE);
        px(cx + x, cy + h + 1, COL_AGED);
    }
    if (open > 150) {
        fillRect(cx + pxo - 3, cy + pyo - 3, 2, 2, COL_WHITE);
    }
}

void drawHuginn(uint32_t now, uint32_t /*tPage*/, const TdUiStatus& st) {
    embers(now, 16, 150);
    drawEye(now, 32, 45);

    char buf[24];
    const int x0 = 66;
    text(x0, 13, "SESSION", COL_AGED);
    hline(x0, 21, 92, COL_DIM);

    text(x0, 24, "W", COL_RUNE);
    snprintf(buf, sizeof(buf), "%d", st.sessWifi);
    text(x0 + 9, 24, buf, COL_BONE);
    text(x0 + 48, 24, "B", COL_STEEL);
    snprintf(buf, sizeof(buf), "%d", st.sessBle);
    text(x0 + 57, 24, buf, COL_BONE);

    text(x0, 34, "Z", COL_MOSS);
    snprintf(buf, sizeof(buf), "%d", st.sessZigbee);
    text(x0 + 9, 34, buf, COL_BONE);
    int omens = st.flippers + st.airtags + st.skimmers;
    text(x0 + 48, 34, "!", omens ? COL_BLOOD : COL_ASH);
    snprintf(buf, sizeof(buf), "%d", omens);
    text(x0 + 57, 34, buf, omens ? COL_BLOOD : COL_ASH);

    snprintf(buf, sizeof(buf), "F%d A%d S%d", st.flippers, st.airtags, st.skimmers);
    text(x0, 44, buf, omens ? COL_BLOOD : COL_ASH);

    uint32_t s = now / 1000;
    snprintf(buf, sizeof(buf), "UP %02u:%02u:%02u",
             (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));
    text(x0, 56, buf, COL_BRONZE);

    snprintf(buf, sizeof(buf), "HEAP %uK", (unsigned)st.freeHeapK);
    text(x0, 66, buf, COL_ASH);
    snprintf(buf, sizeof(buf), "v%s", st.fw ? st.fw : "?");
    textRight(158, 66, buf, COL_DIM);
}

void drawPage(int page, uint32_t now, uint32_t tPage, const TdUiStatus& st) {
    switch (page) {
        case PAGE_RADAR:    drawRadar(now, tPage, st); break;
        case PAGE_SPECTRUM: drawSpectrum(now, tPage, st); break;
        case PAGE_TARGETS:  drawTargets(now, tPage, st); break;
        case PAGE_HUGINN:   drawHuginn(now, tPage, st); break;
        default: break;
    }
}

// ---------------------------------------------------------------------
//  Boot splash
// ---------------------------------------------------------------------

// The crow crosses the screen left-to-right during the first part of the
// splash; the title letters land as it passes over them.
const uint32_t CROW_MS = 2300;
inline uint32_t crowTimeAt(int x) { return (uint32_t)((x + 34) * (int)CROW_MS / (W + 72)); }

// Anti-aliased polygon fill (even-odd), vertices in 1/16 px. 4 sub-scanlines
// per pixel row with exact horizontal span coverage, shaded with a vertical
// gradient from `top` (at yTop px) to `bot` (at yBot px).
void fillPolyAA(const int (*pt)[2], int n, uint16_t top, uint16_t bot, int yTop, int yBot) {
    int minY = pt[0][1], maxY = pt[0][1];
    for (int i = 1; i < n; i++) {
        if (pt[i][1] < minY) minY = pt[i][1];
        if (pt[i][1] > maxY) maxY = pt[i][1];
    }
    int py0 = (minY >> 4) + g_oy, py1 = (maxY >> 4) + g_oy;
    if (py0 < 0) py0 = 0;
    if (py1 > H - 1) py1 = H - 1;
    static uint8_t cov[TD_UI_W];
    for (int py = py0; py <= py1; py++) {
        memset(cov, 0, sizeof(cov));
        for (int sub = 0; sub < 4; sub++) {
            const int sy = ((py - g_oy) << 4) + 2 + sub * 4;      // sample row, 1/16 px
            int xs[16], nx = 0;
            for (int i = 0; i < n && nx < 16; i++) {
                const int* a = pt[i];
                const int* b = pt[(i + 1) % n];
                if ((a[1] <= sy) == (b[1] <= sy)) continue;
                xs[nx++] = a[0] + (b[0] - a[0]) * (sy - a[1]) / (b[1] - a[1]);
            }
            for (int i = 1; i < nx; i++)                           // insertion sort
                for (int k = i; k > 0 && xs[k - 1] > xs[k]; k--) { int t = xs[k]; xs[k] = xs[k - 1]; xs[k - 1] = t; }
            for (int i = 0; i + 1 < nx; i += 2) {
                int xa = xs[i] + (g_ox << 4), xb = xs[i + 1] + (g_ox << 4);
                if (xa < 0) xa = 0;
                if (xb > (W << 4)) xb = W << 4;
                for (int x = xa >> 4; x <= (xb - 1) >> 4 && xa < xb; x++) {
                    int l = x << 4, r = l + 16;
                    int o = (xb < r ? xb : r) - (xa > l ? xa : l);
                    if (o > 0) cov[x] += (uint8_t)o;               // max 4 * 16 = 64
                }
            }
        }
        int g = (yBot > yTop) ? clampi((py - g_oy - yTop) * 256 / (yBot - yTop), 0, 256) : 0;
        const uint16_t c = mix565(top, bot, g);
        uint16_t* row = g_fb + py * W;
        for (int x = 0; x < W; x++)
            if (cov[x]) row[x] = mix565(row[x], c, cov[x] * 4);
    }
}

// Raven in level flight, flying right, body centred on (cx, cy) in 1/16 px.
// `wing` is the wing position in Q8: +256 = fully raised, -256 = fully lowered.
// Drawn as a moonlit silhouette: three anti-aliased polygons (far wing, body,
// near wing) so it stays smooth at this resolution and at sub-pixel positions.
void drawRaven(int cx, int cy, int wing) {
    // Body outline, clockwise from the beak tip (px, y down).
    static const int8_t BODY[][2] = {
        { 27, -1 }, { 22, -3 }, { 19, -6 }, { 15, -7 }, { 11, -6 }, { 4, -5 },
        { -6, -4 }, { -14, -3 }, { -24, -4 }, { -31, -2 }, { -32, 1 }, { -25, 2 },
        { -14, 3 }, { -5, 5 }, { 5, 5 }, { 12, 3 }, { 17, 1 }, { 21, 0 },
    };
    // Raised wing: root along the back, long hand with fingered primaries.
    static const int8_t WING[][2] = {
        { 9, -4 }, { 7, -13 }, { 2, -22 }, { -5, -30 }, { -6, -25 }, { -10, -29 },
        { -10, -23 }, { -14, -26 }, { -14, -20 }, { -18, -22 }, { -17, -15 },
        { -19, -9 }, { -15, -3 },
    };
    const int NB = sizeof(BODY) / sizeof(BODY[0]);
    const int NW = sizeof(WING) / sizeof(WING[0]);
    const uint16_t HI = RGB(0x5a6a94), MID = RGB(0x262e46), LO = RGB(0x0e121c);

    int pts[18][2];
    const int px0 = cx >> 4, py0 = cy >> 4;

    // The wing is the raised shape scaled vertically by `wing`; the downstroke
    // reaches a little less far and sweeps the tip forward.
    const int amp = wing >= 0 ? wing : wing * 3 / 4;
    auto buildWing = [&](int offX, int offY, int scale) {
        for (int i = 0; i < NW; i++) {
            int x = WING[i][0], y = WING[i][1] + 4;           // relative to the root line
            int sweep = (256 - wing) * (-y) / 26 * 3 / 256;   // 0..~7 px forward at the tip
            pts[i][0] = cx + offX * 16 + (x + sweep) * scale / 16;
            pts[i][1] = cy + offY * 16 + (-64 + y * amp / 16) * scale / 256;
        }
    };

    buildWing(4, 0, 220);                                    // far wing, smaller + darker
    fillPolyAA(pts, NW, MID, LO, py0 - 30, py0 + 24);

    for (int i = 0; i < NB; i++) {
        pts[i][0] = cx + BODY[i][0] * 16;
        pts[i][1] = cy + BODY[i][1] * 16;
    }
    fillPolyAA(pts, NB, HI, LO, py0 - 9, py0 + 4);

    buildWing(0, 0, 256);                                    // near wing
    fillPolyAA(pts, NW, HI, MID, py0 - 30, py0 + 24);

    px(px0 + 17, py0 - 4, COL_RUNE);                         // eye
}

void drawSplash(uint32_t t, const TdUiStatus& st) {
    embers(t + 4000, 26, 256);

    // "HUGINN" — letters drop in one by one, flashing white as they land.
    const char* title = "HUGINN";
    const int sc = 3, x0 = (W - (6 * 6 * sc - sc)) / 2, yT = 14;
    for (int i = 0; i < 6; i++) {
        int32_t lt = (int32_t)t - (int32_t)crowTimeAt(x0 + i * 6 * sc + 8);
        if (lt < 0) continue;
        float e = easeOut(lt / 380.0f);
        int y = yT - (int)((1.0f - e) * 40);
        uint16_t c = lt < 420 ? mix565(COL_WHITE, COL_BRONZE, (int)(lt * 256 / 420)) : COL_BRONZE;
        char ch[2] = { title[i], 0 };
        text(x0 + i * 6 * sc + 1, y + 1, ch, scale565(COL_BLOOD, 90), sc);   // ember shadow
        text(x0 + i * 6 * sc, y, ch, c, sc);
    }

    // Rule growing from the centre.
    if (t > 1800) {
        int w = (int)(easeOut((t - 1800) / 450.0f) * 62);
        hline(W / 2 - w, 41, 2 * w, COL_AGED);
        hline(W / 2 - w / 2, 42, w, COL_BRONZE);
        px(W / 2 - w - 2, 41, COL_RUNE);
        px(W / 2 + w + 1, 41, COL_RUNE);
    }

    // Subtitle types on.
    if (t > 2050) {
        const char* sub = "RAVEN OF THOUGHT";
        int n = clampi((int)((t - 2050) / 45), 0, (int)strlen(sub));
        char buf[20];
        snprintf(buf, sizeof(buf), "%.*s", n, sub);
        int sx = (W - textW(sub) + 1) / 2;
        int ex = text(sx, 48, buf, COL_ASH);
        if (n < (int)strlen(sub) || ((t / 200) & 1)) fillRect(ex, 48, 4, 7, COL_RUNE);
    }

    // Awakening bar.
    if (t > 900) {
        const int bw = 100, bx = (W - bw) / 2, by = 66;
        rect(bx - 2, by - 2, bw + 4, 7, COL_DIM);
        int w = clampi((int)((t - 900) * bw / (SPLASH_MS - 1300)), 0, bw);
        for (int x = 0; x < w; x++) {
            uint16_t c = mix565(COL_AGED, COL_RUNE, x * 256 / bw);
            if (x > w - 4) c = COL_WHITE;
            vline(bx + x, by, 3, c);
        }
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "v%s", st.fw ? st.fw : "?");
    textRight(158, 72, buf, COL_DIM);

    // The raven: a slow, heavy wingbeat (about one beat a second) with a
    // gentle rise and fall, and a few sparks shaken loose in its wake.
    if (t < CROW_MS + 300) {
        int cx16 = -34 * 16 + (int)((uint64_t)t * (W + 72) * 16 / CROW_MS);
        int beat = (int)(t * 1024 / 1050);
        int wing = (icos(beat) * 256) >> 12;                       // +256 up .. -256 down
        // The body bobs against the wing: sinks as the wings rise.
        int cy16 = 31 * 16 + ((isin(beat + 256) * 40) >> 12);
        for (int k = 0; k < 6; k++) {
            uint32_t h = hash32(k * 7919 + t / 110);
            int sx = (cx16 >> 4) - 30 - (int)(h % 30);
            int sy = (cy16 >> 4) + (int)((h >> 8) % 9) - 3 + (int)((h % 30) / 5);
            pxMax(sx, sy, scale565(COL_RUNE, 200 - (int)(h % 30) * 6));
        }
        drawRaven(cx16, cy16, wing);
    }

    // A glitch burst as the splash hands over to the first page.
    if (t > SPLASH_MS - 160)
        glitch(t / 40, 5, 7);
}

// ---------------------------------------------------------------------
//  Alert takeover
// ---------------------------------------------------------------------
void drawAlert(uint32_t now, uint32_t t) {
    // Pulsing blood wash.
    int pulse = 40 + ((isin((int)(t * 1024 / 500)) * 28) >> 12);
    uint16_t wash = scale565(COL_BLOOD, pulse);
    for (int i = 0; i < W * H; i++) g_fb[i] = wash;

    // Marching hazard stripes, top and bottom.
    int march = (int)(t / 35);
    for (int y = 0; y < 7; y++) {
        for (int x = 0; x < W; x++) {
            bool on = (((x + y + march) / 6) & 1) != 0;
            uint16_t c = on ? COL_BLOOD : 0x0000;
            g_fb[y * W + x] = c;
            g_fb[(H - 1 - y) * W + (W - 1 - x)] = c;
        }
    }

    // Type, with a chromatic-split flicker.
    char type[14];
    snprintf(type, sizeof(type), strlen(g_alert.type) <= 8 ? "!%s!" : "%s", g_alert.type);
    int jit = ((t / 70) % 5 == 0) ? 2 : 1;
    textCenter(W / 2 - jit, 14, type, COL_BLOOD, 2);
    textCenter(W / 2 + jit, 14, type, RGB(0x1890c0), 2);
    textCenter(W / 2, 14, type, COL_WHITE, 2);

    // Detail lines slide up into place.
    float e = easeOut(t / 300.0f);
    int dy = (int)((1.0f - e) * 20);
    textCenter(W / 2, 36 + dy, g_alert.detail, COL_BONE);
    if (g_alert.rssi != 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "RSSI %d dBm", g_alert.rssi);
        textCenter(W / 2, 48 + dy, buf, COL_RUNE);
        // Proximity meter.
        int w = clampi((g_alert.rssi + 98) * 120 / 68, 4, 120);
        w = (int)(w * e);
        rect(19, 59, 122, 5, scale565(COL_BONE, 120));
        fillRect(20, 60, w, 3, COL_BONE);
    }

    // Time-remaining bar.
    int left = (int)((ALERT_MS - t) * W / ALERT_MS);
    hline(0, 71, left, COL_RUNE);

    if ((t / 90) % 4 == 0) glitch(now / 45, 4, 6);
}

void startAlert(uint32_t now) {
    g_alert.pending = false;
    g_alert.active  = true;
    g_alert.start   = now;
}

} // namespace

// =====================================================================
//  Public API
// =====================================================================

void td_ui_init() {
    if (!g_sinReady) {
        for (int i = 0; i < 1024; i++)
            g_sin[i] = (int16_t)lroundf(sinf((float)i * 6.2831853f / 1024.0f) * 4096.0f);
        g_sinReady = true;
    }
    memset(g_wifi, 0, sizeof(g_wifi));
    memset(g_ble, 0, sizeof(g_ble));
    memset(&g_alert, 0, sizeof(g_alert));
    memset(g_alerted, 0, sizeof(g_alerted));
    g_alertedNext = 0;
    memset(g_barCur, 0, sizeof(g_barCur));
    memset(g_barPeak, 0, sizeof(g_barPeak));
    g_dispWifi = g_dispBle = g_dispThird = 0;
    g_started  = false;
    g_page     = PAGE_RADAR;
    g_prevPage = -1;
    g_pageStart = 0;
    g_nextReq  = false;
    g_tickerPx = 0;
}

void td_ui_note_wifi(uint32_t nowMs, uint32_t hash, const char* ssid,
                     int rssi, int channel, TdUiSec sec) {
    bool isNew;
    Dev* d = devSlot(g_wifi, MAX_WIFI, hash, nowMs, isNew);
    d->last    = nowMs;
    d->rssi    = (int8_t)clampi(rssi, -127, 0);
    d->channel = (uint8_t)clampi(channel, 0, 255);
    d->sec     = (uint8_t)sec;
    snprintf(d->name, sizeof(d->name), "%s", ssid ? ssid : "");
}

void td_ui_note_ble(uint32_t nowMs, uint32_t hash, const char* mac,
                    int rssi, uint8_t flags) {
    bool isNew;
    Dev* d = devSlot(g_ble, MAX_BLE, hash, nowMs, isNew);
    d->last   = nowMs;
    d->rssi   = (int8_t)clampi(rssi, -127, 0);
    d->flags |= flags;
    if (!flags) return;

    // Alert once per device per session. The live table forgets a device after
    // BLE_EXPIRE_MS (or under eviction), so "new to the table" is not enough —
    // remember what has already been announced.
    for (int i = 0; i < ALERTED_MAX; i++)
        if (g_alerted[i].hash == hash && (g_alerted[i].flags & flags) == flags) return;
    Alerted& a = g_alerted[g_alertedNext];
    g_alertedNext = (g_alertedNext + 1) % ALERTED_MAX;
    a.hash  = hash;
    a.flags = flags;
    const char* type = (flags & TD_BLE_SKIMMER) ? "SKIMMER"
                     : (flags & TD_BLE_FLIPPER) ? "FLIPPER" : "TRACKER";
    td_ui_alert(nowMs, type, mac, rssi);
}

void td_ui_alert(uint32_t nowMs, const char* type, const char* detail, int rssi) {
    snprintf(g_alert.type, sizeof(g_alert.type), "%s", type ? type : "ALERT");
    snprintf(g_alert.detail, sizeof(g_alert.detail), "%s", detail ? detail : "");
    g_alert.rssi    = rssi;
    g_alert.pending = true;
    g_alert.active  = false;
    g_alert.start   = nowMs;
}

void td_ui_next_page() {
    g_nextReq = true;
}

void td_ui_render(uint16_t* fb, const TdUiStatus& st) {
    g_fb = fb;
    g_ox = g_oy = 0;
    const uint32_t now = st.nowMs;
    if (!g_sinReady) td_ui_init();
    if (!g_started) { g_started = true; g_boot = now; }
    const uint32_t t = now - g_boot;

    for (int i = 0; i < W * H; i++) fb[i] = COL_BG;

    // ---- Housekeeping that runs on every frame, whatever is on screen ----
    expire(g_wifi, MAX_WIFI, now, WIFI_EXPIRE_MS);
    expire(g_ble, MAX_BLE, now, BLE_EXPIRE_MS);

    int   omens = st.flippers + st.airtags + st.skimmers;
    bool  zig   = st.mode && strcmp(st.mode, "zigbee") == 0;
    float tw = (float)countUsed(g_wifi, MAX_WIFI);
    float tb = (float)countUsed(g_ble, MAX_BLE);
    float t3 = (float)(zig ? st.sessZigbee : omens);
    g_dispWifi  += (tw - g_dispWifi) * 0.16f;
    g_dispBle   += (tb - g_dispBle) * 0.16f;
    g_dispThird += (t3 - g_dispThird) * 0.16f;

    // Spectrum bars ease toward the strongest AP on each channel.
    int best[NUM_CH], cnt[NUM_CH];
    for (int i = 0; i < NUM_CH; i++) { best[i] = -128; cnt[i] = 0; }
    for (int i = 0; i < MAX_WIFI; i++) {
        if (!g_wifi[i].used) continue;
        int ci = channelIndex(g_wifi[i].channel);
        if (ci < 0) continue;
        cnt[ci]++;
        if (g_wifi[i].rssi > best[ci]) best[ci] = g_wifi[i].rssi;
    }
    for (int i = 0; i < NUM_CH; i++) {
        float target = 0;
        if (cnt[i]) target = (float)clampi((best[i] + 98) * 40 / 66 + (cnt[i] - 1) * 2 + 4, 4, 46);
        g_barCur[i] += (target - g_barCur[i]) * 0.22f;
        if (g_barCur[i] > g_barPeak[i]) g_barPeak[i] = g_barCur[i];
        else                            g_barPeak[i] -= 0.3f;
        if (g_barPeak[i] < 0) g_barPeak[i] = 0;
    }

    // ---- Splash ----
    if (t < SPLASH_MS) {
        drawSplash(t, st);
        return;
    }
    if (g_prevPage == -1 && g_pageStart == 0) g_pageStart = now;   // first page

    // ---- Alert takeover ----
    if (g_alert.pending) startAlert(now);
    if (g_alert.active) {
        uint32_t at = now - g_alert.start;
        if (at < ALERT_MS) {
            drawAlert(now, at);
            return;
        }
        g_alert.active = false;
        g_pageStart = now;             // don't flip page the instant it clears
    }

    // ---- Page rotation ----
    if (g_nextReq || (now - g_pageStart) > PAGE_ROTATE_MS) {
        g_nextReq   = false;
        g_prevPage  = g_page;
        g_page      = (g_page + 1) % PAGE_COUNT;
        g_pageStart = now;
    }

    uint32_t tPage = now - g_pageStart;
    if (tPage < PAGE_TRANS_MS) {
        float e = easeOut((float)tPage / PAGE_TRANS_MS);
        int shift = (int)(e * W);
        if (g_prevPage >= 0) {
            g_ox = -shift;
            drawPage(g_prevPage, now, PAGE_ROTATE_MS, st);
        }
        g_ox = W - shift;
        drawPage(g_page, now, tPage, st);
        g_ox = 0;
    } else {
        drawPage(g_page, now, tPage, st);
    }

    drawHeader(now, st);
}
