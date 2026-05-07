#include "display_manager.h"
#include "serial_cmd.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "scan_cycle.h"
#include "config.h"

#include <Wire.h>
#include <Arduino_GFX_Library.h>

// =====================================================================
//  Arduino_GFX display for Waveshare ESP32-S3-Touch-LCD-4B
//  ST7701 480x480 RGB panel via TCA9554 I2C GPIO expander
// =====================================================================
static Arduino_XCA9554SWSPI *expander = new Arduino_XCA9554SWSPI(
    7, 0, 2, 1, &Wire, 0x20);

static Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    17 /*DE*/, 3 /*VSYNC*/, 46 /*HSYNC*/, 9 /*PCLK*/,
    10 /*B0*/, 11 /*B1*/, 12 /*B2*/, 13 /*B3*/, 14 /*B4*/,
    21 /*G0*/, 8 /*G1*/, 18 /*G2*/, 45 /*G3*/, 38 /*G4*/, 39 /*G5*/,
    40 /*R0*/, 41 /*R1*/, 42 /*R2*/, 2 /*R3*/, 1 /*R4*/,
    1, 10, 8, 50,   // hsync timing
    1, 10, 8, 20);  // vsync timing

static Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480, 480, rgbpanel, 0, true,
    expander, GFX_NOT_DEFINED,
    st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

// =====================================================================
//  Viking palette (RGB565)
//  Mirrors the web flasher: ink, pitch, bone, bronze, rune, blood, steel.
// =====================================================================
static const uint16_t COL_INK     = 0x0000;  // pure black
static const uint16_t COL_PITCH   = 0x18A2;  // very dark warm (#15110d)
static const uint16_t COL_HEADER  = 0x20E2;  // header bar (#221d15)
static const uint16_t COL_BONE    = 0xEF1A;  // pale bone text (#e8e2d4)
static const uint16_t COL_BRONZE  = 0xCD0B;  // primary accent (#c8a35d)
static const uint16_t COL_RUNE    = 0xDDCF;  // bright gold highlight (#d9b878)
static const uint16_t COL_AGED    = 0x8B67;  // dim bronze (#8a6f3a)
static const uint16_t COL_ASH     = 0x6B0B;  // cool dim grey-brown (#6b6358)
static const uint16_t COL_BLOOD   = 0xC9C7;  // bright blood red (#cb3b3b)
static const uint16_t COL_BLOOD_D = 0x8924;  // dark blood (#8b2626)
static const uint16_t COL_STEEL   = 0x6D59;  // cold ice steel (#6fa8c8)

// Threat color mapping — warm/Norse where it makes sense.
static const uint16_t COL_FLIPPER   = COL_STEEL;   // cold steel
static const uint16_t COL_AIRTAG    = COL_BRONZE;  // watching eye
static const uint16_t COL_SKIMMER   = COL_BLOOD;   // blood
static const uint16_t COL_PINEAPPLE = COL_RUNE;    // rune-gold
static const uint16_t COL_SPAM      = COL_ASH;     // ash

// ---- Alert ring buffer ----
struct AlertEntry {
    char type[16];
    char mac[24];
    int  rssi;
    unsigned long timestamp;
};

#define MAX_ALERTS 8
static AlertEntry s_alerts[MAX_ALERTS];
static int s_alertHead = 0;
static int s_alertCount = 0;
static SemaphoreHandle_t s_alertMutex = nullptr;

// ---- Button definitions ----
struct Button {
    int16_t x, y, w, h;
    const char* label;
    ScanMode mode;
};

static const Button BUTTONS[] = {
    {  10, 430, 88, 40, "WiFi", MODE_WIFI         },
    { 103, 430, 88, 40, "BLE",  MODE_BLE_ALL      },
    { 196, 430, 88, 40, "Flpr", MODE_BLE_FILTERED },
    { 289, 430, 88, 40, "Halt", MODE_IDLE          },
    { 382, 430, 88, 40, "Auto", MODE_AUTO_CYCLE    },
};
static const int NUM_BUTTONS = sizeof(BUTTONS) / sizeof(BUTTONS[0]);

// =====================================================================
//  Ornament helpers
// =====================================================================
static void drawDiamond(int16_t cx, int16_t cy, int16_t s, uint16_t color) {
    gfx->fillTriangle(cx, cy - s, cx - s, cy, cx + s, cy, color);
    gfx->fillTriangle(cx, cy + s, cx - s, cy, cx + s, cy, color);
}

// Section heading: bronze label + thin gradient-ish underline.
static void drawSectionHeader(int16_t x, int16_t y, const char* label) {
    gfx->setTextColor(COL_BRONZE);
    gfx->setTextSize(2);
    gfx->setCursor(x, y);
    gfx->print(label);

    int16_t lineY = y + 22;
    gfx->drawFastHLine(x, lineY, 80, COL_AGED);
    gfx->drawFastHLine(x + 84, lineY, 4, COL_BRONZE);
    gfx->drawFastHLine(x + 92, lineY, SCREEN_WIDTH - x - 102, COL_AGED);
}

// ---- Helper: uptime string ----
static void uptimeStr(char* buf, size_t len) {
    unsigned long s = millis() / 1000;
    unsigned long m = s / 60;
    unsigned long h = m / 60;
    snprintf(buf, len, "%02lu:%02lu:%02lu", h, m % 60, s % 60);
}

// =====================================================================
//  Status bar
// =====================================================================
static void drawStatusBar() {
    gfx->fillRect(0, 0, SCREEN_WIDTH, 44, COL_HEADER);
    // bronze rule under header
    gfx->drawFastHLine(0, 44, SCREEN_WIDTH, COL_AGED);
    gfx->drawFastHLine(0, 45, SCREEN_WIDTH, COL_PITCH);

    // brand mark, left
    gfx->setTextColor(COL_BRONZE);
    gfx->setTextSize(2);
    gfx->setCursor(10, 12);
    gfx->print("HUGINNESP");

    // diamond ornament between brand and stats
    drawDiamond(140, 22, 3, COL_AGED);

    // counters, middle
    char buf[64];
    snprintf(buf, sizeof(buf), "W:%d  B:%d  %s",
             wifi_scanner_count(), ble_scanner_count(),
             scanModeName(g_currentMode));
    gfx->setTextColor(COL_BONE);
    gfx->setCursor(160, 12);
    gfx->print(buf);

    // uptime, right
    char up[16];
    uptimeStr(up, sizeof(up));
    gfx->setTextColor(COL_AGED);
    gfx->setCursor(SCREEN_WIDTH - 110, 12);
    gfx->print(up);
}

// =====================================================================
//  Buttons (engraved bronze plates)
// =====================================================================
static void drawButtons() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        const Button& b = BUTTONS[i];
        bool active = (g_currentMode == b.mode);

        if (active) {
            // active: filled bronze with ink text
            gfx->fillRoundRect(b.x, b.y, b.w, b.h, 4, COL_BRONZE);
            gfx->drawRoundRect(b.x, b.y, b.w, b.h, 4, COL_RUNE);
            gfx->setTextColor(COL_INK);
        } else {
            // inactive: pitch fill, aged bronze border
            gfx->fillRoundRect(b.x, b.y, b.w, b.h, 4, COL_PITCH);
            gfx->drawRoundRect(b.x, b.y, b.w, b.h, 4, COL_AGED);
            gfx->setTextColor(COL_BONE);
        }

        gfx->setTextSize(2);
        int16_t tx = b.x + (b.w - (int)strlen(b.label) * 12) / 2;
        int16_t ty = b.y + 12;
        gfx->setCursor(tx, ty);
        gfx->print(b.label);
    }
}

// =====================================================================
//  Stats panel
// =====================================================================
static void drawStats() {
    gfx->fillRect(0, 48, SCREEN_WIDTH, 378, COL_INK);

    // ---- WiFi section ----
    drawSectionHeader(10, 58, "WiFi Sightings");

    int wifiCount = 0;
    const WifiNetwork* nets = wifi_scanner_get_networks(wifiCount);

    int openCount = 0, wepCount = 0, wpaCount = 0, wpa2Count = 0, wpa3Count = 0;
    int band24 = 0, band5 = 0;
    int bestRssi = -999;
    String bestSsid = "";

    for (int i = 0; i < wifiCount; i++) {
        const WifiNetwork& n = nets[i];
        if (n.security == "Open")            openCount++;
        else if (n.security == "WEP")        wepCount++;
        else if (n.security == "WPA")        wpaCount++;
        else if (n.security == "WPA2" || n.security == "WPA/WPA2") wpa2Count++;
        else if (n.security == "WPA3")       wpa3Count++;

        if (n.channel <= 14) band24++;
        else                 band5++;

        if (n.rssi > bestRssi) {
            bestRssi = n.rssi;
            bestSsid = n.ssid;
        }
    }

    int y = 92;
    char buf[64];
    gfx->setTextSize(2);

    gfx->setTextColor(COL_BONE);
    snprintf(buf, sizeof(buf), "BSSID: %d", wifiCount);
    gfx->setCursor(10, y); gfx->print(buf); y += 22;

    snprintf(buf, sizeof(buf), "2.4GHz: %d   5GHz: %d", band24, band5);
    gfx->setCursor(10, y); gfx->print(buf); y += 22;

    // Security row
    gfx->setCursor(10, y);
    if (openCount > 0) {
        gfx->setTextColor(COL_BLOOD);
        snprintf(buf, sizeof(buf), "Open:%d ", openCount);
        gfx->print(buf);
    }
    gfx->setTextColor(COL_BONE);
    if (wepCount > 0) {
        snprintf(buf, sizeof(buf), "WEP:%d ", wepCount);
        gfx->print(buf);
    }
    snprintf(buf, sizeof(buf), "WPA:%d WPA2:%d", wpaCount, wpa2Count);
    gfx->print(buf);
    if (wpa3Count > 0) {
        snprintf(buf, sizeof(buf), " WPA3:%d", wpa3Count);
        gfx->print(buf);
    }
    y += 22;

    if (wifiCount > 0) {
        gfx->setTextColor(COL_RUNE);
        gfx->setCursor(10, y);
        snprintf(buf, sizeof(buf), "Strongest: %ddBm %s",
                 bestRssi, bestSsid.substring(0, 18).c_str());
        gfx->print(buf);
    }
    y += 30;

    // ---- BLE section ----
    drawSectionHeader(10, y, "BLE Sightings");
    y += 34;

    gfx->setTextSize(2);
    gfx->setTextColor(COL_BONE);
    snprintf(buf, sizeof(buf), "Total: %d", ble_scanner_count());
    gfx->setCursor(10, y); gfx->print(buf); y += 22;

    int fc = ble_scanner_flipper_count();
    int ac = ble_scanner_airtag_count();
    int sc = ble_scanner_skimmer_count();

    if (fc > 0) {
        gfx->setTextColor(COL_FLIPPER);
        snprintf(buf, sizeof(buf), "Flipper: %d", fc);
        gfx->setCursor(10, y); gfx->print(buf); y += 22;
    }
    if (ac > 0) {
        gfx->setTextColor(COL_AIRTAG);
        snprintf(buf, sizeof(buf), "AirTag:  %d", ac);
        gfx->setCursor(10, y); gfx->print(buf); y += 22;
    }
    if (sc > 0) {
        gfx->setTextColor(COL_SKIMMER);
        snprintf(buf, sizeof(buf), "Skimmer: %d", sc);
        gfx->setCursor(10, y); gfx->print(buf); y += 22;
    }
    if (fc == 0 && ac == 0 && sc == 0) {
        gfx->setTextColor(COL_AGED);
        gfx->setCursor(10, y);
        gfx->print("No omens cast");
        y += 22;
    }

    y += 10;

    // ---- Recent omens (last 4 alerts) ----
    if (s_alertMutex) xSemaphoreTake(s_alertMutex, portMAX_DELAY);
    int alertCount = min(s_alertCount, 4);
    if (alertCount > 0) {
        drawSectionHeader(10, y, "Recent Omens");
        y += 30;
        for (int i = 0; i < alertCount; i++) {
            int idx = (s_alertHead - alertCount + i + MAX_ALERTS) % MAX_ALERTS;
            AlertEntry& a = s_alerts[idx];
            uint16_t col = COL_BONE;
            if (strcmp(a.type, "flipper") == 0)        col = COL_FLIPPER;
            else if (strcmp(a.type, "airtag") == 0)    col = COL_AIRTAG;
            else if (strcmp(a.type, "skimmer") == 0)   col = COL_SKIMMER;
            else if (strcmp(a.type, "pineapple") == 0) col = COL_PINEAPPLE;
            else if (strcmp(a.type, "spam") == 0)      col = COL_SPAM;

            // bronze bullet diamond
            drawDiamond(14, y + 7, 3, COL_AGED);

            snprintf(buf, sizeof(buf), "%s %s %ddBm", a.type, a.mac, a.rssi);
            gfx->setTextColor(col);
            gfx->setTextSize(2);
            gfx->setCursor(26, y);
            gfx->print(buf);
            y += 20;
        }
    }
    if (s_alertMutex) xSemaphoreGive(s_alertMutex);

    // ---- Heap info (skald's footnote) ----
    gfx->setTextColor(COL_ASH);
    gfx->setTextSize(1);
    gfx->setCursor(10, 415);
    snprintf(buf, sizeof(buf), "Heap %uK  PSRAM %uK",
             ESP.getFreeHeap() / 1024, ESP.getFreePsram() / 1024);
    gfx->print(buf);
}

// =====================================================================
//  Splash screen
// =====================================================================
static void drawSplash() {
    gfx->fillScreen(COL_INK);

    // top ornament: diamond + bronze rule
    drawDiamond(240, 130, 7, COL_BRONZE);
    gfx->drawFastHLine(70, 158, 340, COL_AGED);
    gfx->drawFastHLine(150, 160, 180, COL_BRONZE);
    gfx->drawFastHLine(70, 162, 340, COL_AGED);

    // title — "HUGINNESP" 9 chars * 6px * 5 = 270px wide, centered at x=105
    gfx->setTextSize(5);
    gfx->setTextColor(COL_BRONZE);
    gfx->setCursor(105, 200);
    gfx->print("HUGINNESP");

    // subtitle in dim bronze, centered
    gfx->setTextSize(2);
    gfx->setTextColor(COL_AGED);
    gfx->setCursor(108, 270);
    gfx->print("RAVEN  OF  THOUGHT");

    // bottom rule + diamond
    gfx->drawFastHLine(70, 300, 340, COL_AGED);
    gfx->drawFastHLine(150, 302, 180, COL_BRONZE);
    gfx->drawFastHLine(70, 304, 340, COL_AGED);
    drawDiamond(240, 332, 7, COL_BRONZE);

    // status
    gfx->setTextSize(2);
    gfx->setTextColor(COL_ASH);
    gfx->setCursor(150, 410);
    gfx->print("Awakening...");
}

// =====================================================================
//  Public API
// =====================================================================

void display_add_alert(const char* type, const char* mac, int rssi) {
    if (s_alertMutex) xSemaphoreTake(s_alertMutex, portMAX_DELAY);
    AlertEntry& a = s_alerts[s_alertHead];
    strncpy(a.type, type, sizeof(a.type) - 1);
    a.type[sizeof(a.type) - 1] = '\0';
    strncpy(a.mac, mac, sizeof(a.mac) - 1);
    a.mac[sizeof(a.mac) - 1] = '\0';
    a.rssi = rssi;
    a.timestamp = millis();
    s_alertHead = (s_alertHead + 1) % MAX_ALERTS;
    if (s_alertCount < MAX_ALERTS) s_alertCount++;
    if (s_alertMutex) xSemaphoreGive(s_alertMutex);
}

void display_init() {
    s_alertMutex = xSemaphoreCreateMutex();

    Wire.begin(47, 48);

    // Reset sequence via TCA9554 GPIO expander
    expander->pinMode(5, OUTPUT);
    expander->pinMode(6, OUTPUT);
    expander->digitalWrite(6, LOW);   // backlight off during reset
    delay(200);
    expander->digitalWrite(5, LOW);
    delay(200);
    expander->digitalWrite(5, HIGH);
    delay(200);

    if (!gfx->begin()) {
        Serial.println("[DISP] gfx->begin() FAILED!");
        return;
    }

    drawSplash();
    delay(1800);
    gfx->fillScreen(COL_INK);
}

void display_task(void* param) {
    for (;;) {
        drawStatusBar();
        drawStats();
        drawButtons();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
