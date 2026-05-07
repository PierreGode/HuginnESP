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
//  ST7701 480×480 RGB panel via TCA9554 I2C GPIO expander
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

// ---- RGB565 colors ----
static const uint16_t COL_BG        = 0x0000;  // black
static const uint16_t COL_HEADER    = 0x1089;  // dark blue-grey
static const uint16_t COL_TEXT      = 0xE71C;  // light grey
static const uint16_t COL_ACCENT    = 0x07F1;  // green
static const uint16_t COL_FLIPPER   = 0x449F;  // blue
static const uint16_t COL_AIRTAG    = 0xFC40;  // orange
static const uint16_t COL_SKIMMER   = 0xF800;  // red
static const uint16_t COL_PINEAPPLE = 0xFE60;  // yellow
static const uint16_t COL_SPAM      = 0xA23F;  // purple
static const uint16_t COL_BTN       = 0x32CA;  // dark grey-blue
static const uint16_t COL_BTN_ACT   = 0x0553;  // active green

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
    {  10, 430, 88, 40, "WiFi",  MODE_WIFI         },
    { 103, 430, 88, 40, "BLE",   MODE_BLE_ALL      },
    { 196, 430, 88, 40, "Flip",  MODE_BLE_FILTERED },
    { 289, 430, 88, 40, "Stop",  MODE_IDLE          },
    { 382, 430, 88, 40, "Auto",  MODE_AUTO_CYCLE    },
};
static const int NUM_BUTTONS = sizeof(BUTTONS) / sizeof(BUTTONS[0]);

// ---- Helper: uptime string ----
static void uptimeStr(char* buf, size_t len) {
    unsigned long s = millis() / 1000;
    unsigned long m = s / 60;
    unsigned long h = m / 60;
    snprintf(buf, len, "%02lu:%02lu:%02lu", h, m % 60, s % 60);
}

// ---- Draw status bar ----
static void drawStatusBar() {
    gfx->fillRect(0, 0, SCREEN_WIDTH, 44, COL_HEADER);

    char buf[64];
    snprintf(buf, sizeof(buf), "WiFi:%d BLE:%d %s",
             wifi_scanner_count(), ble_scanner_count(),
             scanModeName(g_currentMode));
    gfx->setTextColor(COL_ACCENT);
    gfx->setTextSize(2);
    gfx->setCursor(10, 12);
    gfx->print(buf);

    char up[16];
    uptimeStr(up, sizeof(up));
    gfx->setCursor(SCREEN_WIDTH - 110, 12);
    gfx->print(up);
}

// ---- Draw buttons ----
static void drawButtons() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        const Button& b = BUTTONS[i];
        bool active = (g_currentMode == b.mode);
        uint16_t col = active ? COL_BTN_ACT : COL_BTN;
        gfx->fillRoundRect(b.x, b.y, b.w, b.h, 6, col);
        gfx->setTextColor(COL_TEXT);
        gfx->setTextSize(2);
        int16_t tx = b.x + (b.w - strlen(b.label) * 12) / 2;
        int16_t ty = b.y + 12;
        gfx->setCursor(tx, ty);
        gfx->print(b.label);
    }
}

// ---- Draw stats panel ----
static void drawStats() {
    gfx->fillRect(0, 48, SCREEN_WIDTH, 378, COL_BG);

    // ---- WiFi section ----
    gfx->setTextColor(COL_ACCENT);
    gfx->setTextSize(2);
    gfx->setCursor(10, 55);
    gfx->print("WiFi Networks");

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

    gfx->setTextColor(COL_TEXT);
    gfx->setTextSize(2);
    int y = 80;
    char buf[64];

    snprintf(buf, sizeof(buf), "BSSID: %d", wifiCount);
    gfx->setCursor(10, y); gfx->print(buf); y += 22;

    snprintf(buf, sizeof(buf), "2.4GHz: %d   5GHz: %d", band24, band5);
    gfx->setCursor(10, y); gfx->print(buf); y += 22;

    // Security row
    gfx->setCursor(10, y);
    if (openCount > 0) { gfx->setTextColor(COL_SKIMMER); snprintf(buf, sizeof(buf), "Open:%d ", openCount); gfx->print(buf); }
    gfx->setTextColor(COL_TEXT);
    if (wepCount > 0) { snprintf(buf, sizeof(buf), "WEP:%d ", wepCount); gfx->print(buf); }
    snprintf(buf, sizeof(buf), "WPA:%d WPA2:%d", wpaCount, wpa2Count);
    gfx->print(buf);
    if (wpa3Count > 0) { snprintf(buf, sizeof(buf), " WPA3:%d", wpa3Count); gfx->print(buf); }
    y += 22;

    if (wifiCount > 0) {
        gfx->setTextColor(COL_ACCENT);
        gfx->setCursor(10, y);
        snprintf(buf, sizeof(buf), "Best: %ddBm %s", bestRssi, bestSsid.substring(0, 18).c_str());
        gfx->print(buf);
    }
    y += 30;

    // ---- BLE section ----
    gfx->setTextColor(COL_ACCENT);
    gfx->setCursor(10, y);
    gfx->print("BLE Devices");
    y += 25;

    gfx->setTextColor(COL_TEXT);
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
        gfx->setTextColor(COL_ACCENT);
        gfx->setCursor(10, y);
        gfx->print("No threats detected");
        y += 22;
    }

    y += 10;

    // ---- Alert summary (compact) ----
    if (s_alertMutex) xSemaphoreTake(s_alertMutex, portMAX_DELAY);
    int alertCount = min(s_alertCount, 4);  // show last 4 alerts max
    if (alertCount > 0) {
        gfx->setTextColor(COL_PINEAPPLE);
        gfx->setCursor(10, y);
        gfx->print("Recent Alerts:");
        y += 22;
        for (int i = 0; i < alertCount; i++) {
            int idx = (s_alertHead - alertCount + i + MAX_ALERTS) % MAX_ALERTS;
            AlertEntry& a = s_alerts[idx];
            uint16_t col = COL_TEXT;
            if (strcmp(a.type, "flipper") == 0)        col = COL_FLIPPER;
            else if (strcmp(a.type, "airtag") == 0)    col = COL_AIRTAG;
            else if (strcmp(a.type, "skimmer") == 0)   col = COL_SKIMMER;
            else if (strcmp(a.type, "pineapple") == 0) col = COL_PINEAPPLE;
            else if (strcmp(a.type, "spam") == 0)      col = COL_SPAM;

            snprintf(buf, sizeof(buf), "%s %s %ddBm", a.type, a.mac, a.rssi);
            gfx->setTextColor(col);
            gfx->setCursor(10, y); gfx->print(buf);
            y += 20;
        }
    }
    if (s_alertMutex) xSemaphoreGive(s_alertMutex);

    // ---- Heap info ----
    gfx->setTextColor(0x4208); // dim grey
    gfx->setTextSize(1);
    gfx->setCursor(10, 410);
    snprintf(buf, sizeof(buf), "Heap: %uK  PSRAM: %uK", ESP.getFreeHeap()/1024, ESP.getFreePsram()/1024);
    gfx->print(buf);
}

// ---- Touch handling (GT911 via Wire on SDA=47 SCL=48) ----
// Note: Touch not yet integrated with Arduino_GFX — will add in next iteration
// For now the display shows status, buttons are visual only
// Touch support can be added via a GT911 library or manual I2C reads

// ==========================
//  Public API
// ==========================

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
    expander->digitalWrite(5, LOW);   // reset low
    delay(200);
    expander->digitalWrite(5, HIGH);  // reset high
    delay(200);

    if (!gfx->begin()) {
        Serial.println("[DISP] gfx->begin() FAILED!");
        return;
    }

    gfx->fillScreen(COL_BG);
    gfx->setTextSize(3);
    gfx->setTextColor(COL_ACCENT);
    gfx->setCursor(120, 200);
    gfx->print("HuginnESP");
    gfx->setTextSize(2);
    gfx->setTextColor(COL_TEXT);
    gfx->setCursor(130, 250);
    gfx->print("Initializing...");
    delay(1500);
    gfx->fillScreen(COL_BG);
}

void display_task(void* param) {
    for (;;) {
        drawStatusBar();
        drawStats();
        drawButtons();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
