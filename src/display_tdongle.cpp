#include "config.h"

#if HUGINN_HAS_DISPLAY && HUGINN_BOARD_TDONGLE_C5

// =====================================================================
//  LilyGO T-Dongle-C5 display back-end
//
//  0.96" ST7735 IPS, 80x160 used in landscape (160x80), on SPI. The whole
//  frame is rendered into an RGB565 framebuffer by the portable renderer in
//  tdongle_ui.cpp and pushed in one SPI transaction (~25 KB, ~10 ms @ 20 MHz),
//  which gives flicker-free ~30 fps animation.
//
//  Scanners feed sightings through display_note_wifi()/display_note_ble();
//  those and the render pass share one mutex.
// =====================================================================

#include "display_manager.h"
#include "tdongle_ui.h"
#include "serial_cmd.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "zigbee_scanner.h"

#include <SPI.h>
#include <Preferences.h>

// ---- Pins (LilyGO T-Dongle-C5 V1.1) ----
#ifndef TDONGLE_LCD_MOSI
#define TDONGLE_LCD_MOSI 2
#endif
#ifndef TDONGLE_LCD_SCK
#define TDONGLE_LCD_SCK  6
#endif
#ifndef TDONGLE_LCD_CS
#define TDONGLE_LCD_CS   10
#endif
#ifndef TDONGLE_LCD_DC
#define TDONGLE_LCD_DC   3
#endif
#ifndef TDONGLE_LCD_RST
#define TDONGLE_LCD_RST  1
#endif
#ifndef TDONGLE_LCD_BL
#define TDONGLE_LCD_BL   0      // backlight, active LOW
#endif

// 1 or 3 — the two landscape orientations. This is only the first-boot default:
// a double-click on the BOOT button flips it and the choice is saved in NVS.
#ifndef TDONGLE_LCD_ROTATION
#define TDONGLE_LCD_ROTATION 3
#endif
#ifndef TDONGLE_LCD_HZ
#define TDONGLE_LCD_HZ 20000000
#endif
// Backlight brightness 0-255.
#ifndef TDONGLE_LCD_BRIGHTNESS
#define TDONGLE_LCD_BRIGHTNESS 200
#endif
#ifndef TDONGLE_FRAME_MS
#define TDONGLE_FRAME_MS 33
#endif
// Render task priority. The scan cycle, loop() and the other app tasks run at
// 1; anything above that keeps animation from being time-sliced against them.
// It stays far below the Wi-Fi / BLE stack tasks (18+), so radio timing is
// unaffected.
#ifndef TDONGLE_REPORT_MS
#define TDONGLE_REPORT_MS 30000
#endif
#ifndef TDONGLE_TASK_PRIORITY
#define TDONGLE_TASK_PRIORITY 3
#endif

// Panel RAM is 132x162; the 80x160 glass sits at (26,1) — (1,26) in landscape.
static const uint16_t LCD_X_OFF = 1;
static const uint16_t LCD_Y_OFF = 26;

static uint16_t*         s_fb      = nullptr;
static SemaphoreHandle_t s_mutex   = nullptr;
static bool              s_started = false;
static const SPISettings s_spi(TDONGLE_LCD_HZ, MSBFIRST, SPI_MODE0);
static uint8_t           s_rotation  = TDONGLE_LCD_ROTATION;   // 1 or 3
static volatile bool     s_flipReq   = false;                  // set by display_flip()
static volatile bool     s_autoReq   = false;                  // set by display_toggle_auto_cycle()

// ---------------------------------------------------------------------
//  ST7735 low level
// ---------------------------------------------------------------------
static void lcdCmd(uint8_t cmd, const uint8_t* data = nullptr, size_t len = 0) {
    SPI.beginTransaction(s_spi);
    digitalWrite(TDONGLE_LCD_CS, LOW);
    digitalWrite(TDONGLE_LCD_DC, LOW);
    SPI.transfer(cmd);
    if (len) {
        digitalWrite(TDONGLE_LCD_DC, HIGH);
        SPI.writeBytes(data, len);
    }
    digitalWrite(TDONGLE_LCD_CS, HIGH);
    SPI.endTransaction();
}

// MADCTL: landscape (MV) + BGR; rotation 1 = MX|MV, rotation 3 = MY|MV.
static void lcdSetRotation() {
    const uint8_t d[] = { (uint8_t)((s_rotation == 1 ? 0x60 : 0xA0) | 0x08) };
    lcdCmd(0x36, d, sizeof(d));
}

static void lcdInit() {
    pinMode(TDONGLE_LCD_CS, OUTPUT);
    pinMode(TDONGLE_LCD_DC, OUTPUT);
    pinMode(TDONGLE_LCD_RST, OUTPUT);
    digitalWrite(TDONGLE_LCD_CS, HIGH);

    digitalWrite(TDONGLE_LCD_RST, HIGH); delay(20);
    digitalWrite(TDONGLE_LCD_RST, LOW);  delay(20);
    digitalWrite(TDONGLE_LCD_RST, HIGH); delay(120);

    SPI.begin(TDONGLE_LCD_SCK, -1, TDONGLE_LCD_MOSI, -1);

    lcdCmd(0x01); delay(150);                                   // SWRESET
    lcdCmd(0x11); delay(150);                                   // SLPOUT
    { const uint8_t d[] = { 0x01, 0x2C, 0x2D };                   lcdCmd(0xB1, d, sizeof(d)); } // FRMCTR1
    { const uint8_t d[] = { 0x01, 0x2C, 0x2D };                   lcdCmd(0xB2, d, sizeof(d)); } // FRMCTR2
    { const uint8_t d[] = { 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D }; lcdCmd(0xB3, d, sizeof(d)); } // FRMCTR3
    { const uint8_t d[] = { 0x07 };                               lcdCmd(0xB4, d, sizeof(d)); } // INVCTR
    { const uint8_t d[] = { 0xA2, 0x02, 0x84 };                   lcdCmd(0xC0, d, sizeof(d)); } // PWCTR1
    { const uint8_t d[] = { 0xC5 };                               lcdCmd(0xC1, d, sizeof(d)); } // PWCTR2
    { const uint8_t d[] = { 0x0A, 0x00 };                         lcdCmd(0xC2, d, sizeof(d)); } // PWCTR3
    { const uint8_t d[] = { 0x8A, 0x2A };                         lcdCmd(0xC3, d, sizeof(d)); } // PWCTR4
    { const uint8_t d[] = { 0x8A, 0xEE };                         lcdCmd(0xC4, d, sizeof(d)); } // PWCTR5
    { const uint8_t d[] = { 0x0E };                               lcdCmd(0xC5, d, sizeof(d)); } // VMCTR1
    lcdCmd(0x21);                                                 // INVON (IPS panel)
    { const uint8_t d[] = { 0x05 };                               lcdCmd(0x3A, d, sizeof(d)); } // COLMOD: RGB565
    lcdSetRotation();
    { const uint8_t d[] = { 0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
                            0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10 }; lcdCmd(0xE0, d, sizeof(d)); }
    { const uint8_t d[] = { 0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
                            0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10 }; lcdCmd(0xE1, d, sizeof(d)); }
    lcdCmd(0x13); delay(10);                                    // NORON
    lcdCmd(0x29); delay(50);                                    // DISPON
}

// Push the whole framebuffer. Byte-swaps in place (the panel wants big-endian
// RGB565); the renderer redraws every pixel next frame, so that is harmless.
static void lcdPush() {
    const uint16_t x0 = LCD_X_OFF, x1 = LCD_X_OFF + TD_UI_W - 1;
    const uint16_t y0 = LCD_Y_OFF, y1 = LCD_Y_OFF + TD_UI_H - 1;
    { const uint8_t d[] = { (uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1 }; lcdCmd(0x2A, d, 4); }
    { const uint8_t d[] = { (uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1 }; lcdCmd(0x2B, d, 4); }

    const size_t n = (size_t)TD_UI_W * TD_UI_H;
    for (size_t i = 0; i < n; i++) s_fb[i] = __builtin_bswap16(s_fb[i]);
    lcdCmd(0x2C, (const uint8_t*)s_fb, n * 2);
}

static inline void backlight(uint8_t level) {
    ledcWrite(TDONGLE_LCD_BL, 255 - level);     // active LOW
}

// ---------------------------------------------------------------------
//  Feed (called from the scan / BLE tasks)
// ---------------------------------------------------------------------
static uint32_t fnv1a(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

void display_note_wifi(const uint8_t bssid[6], const char* ssid, int rssi,
                       int channel, const char* security) {
    if (!s_mutex) return;
    TdUiSec sec = TD_SEC_WPA;
    if (security) {
        if (strcmp(security, "Open") == 0)      sec = TD_SEC_OPEN;
        else if (strcmp(security, "WEP") == 0)  sec = TD_SEC_WEP;
        else if (strstr(security, "WPA3"))      sec = TD_SEC_WPA3;
    }
    const uint32_t h = fnv1a(bssid, 6);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    td_ui_note_wifi(millis(), h, ssid, rssi, channel, sec);
    xSemaphoreGive(s_mutex);
}

void display_note_ble(const char* mac, int rssi,
                      bool flipper, bool airtag, bool skimmer) {
    if (!s_mutex || !mac) return;
    const uint8_t flags = (flipper ? TD_BLE_FLIPPER : 0) | (airtag ? TD_BLE_AIRTAG : 0)
                        | (skimmer ? TD_BLE_SKIMMER : 0);
    const uint32_t h = fnv1a((const uint8_t*)mac, strlen(mac));
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    td_ui_note_ble(millis(), h, mac, rssi, flags);
    xSemaphoreGive(s_mutex);
}

void display_note_zigbee(const char* key, const char* panid, const char* proto,
                         int rssi, int channel) {
    if (!s_mutex || !key) return;
    char label[20];
    snprintf(label, sizeof(label), "%s %s", proto ? proto : "802.15.4", panid ? panid : "");
    const uint32_t h = fnv1a((const uint8_t*)key, strlen(key));
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    td_ui_note_zigbee(millis(), h, label, rssi, channel);
    xSemaphoreGive(s_mutex);
}

void display_add_alert(const char* type, const char* mac, int rssi) {
    if (!s_mutex) return;
    char up[12];
    size_t i = 0;
    for (; type && type[i] && i < sizeof(up) - 1; i++) up[i] = (char)toupper((unsigned char)type[i]);
    up[i] = '\0';
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    td_ui_alert(millis(), up, mac, rssi);
    xSemaphoreGive(s_mutex);
}

void display_toggle_auto_cycle() {
    s_autoReq = true;      // applied by the render task (it also saves to NVS)
}

void display_flip() {
    s_flipReq = true;      // applied by the render task, which owns the SPI bus
}

void display_next_page() {
    if (!s_mutex) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    td_ui_next_page();
    xSemaphoreGive(s_mutex);
}

// ---------------------------------------------------------------------
//  Render task
// ---------------------------------------------------------------------
void display_task(void*) {
    TickType_t wake = xTaskGetTickCount();
    uint32_t frames = 0, lastReport = millis(), busyUs = 0;
    uint32_t prevStart = 0, maxGapUs = 0, maxBusyUs = 0, late = 0;
    const uint32_t t0 = millis();

    for (;;) {
        const uint32_t start = micros();
        if (prevStart) {
            const uint32_t gap = start - prevStart;
            if (gap > maxGapUs) maxGapUs = gap;
            if (gap > 50000) late++;               // a visibly dropped frame
        }
        prevStart = start;

        // Session counters take their own locks — sample them outside ours.
        TdUiStatus st;
        st.nowMs      = millis();
        st.mode       = scanModeName(g_currentMode);
        st.sessWifi   = wifi_scanner_session_count();
        st.sessBle    = ble_scanner_session_count();
        st.sessZigbee = zigbee_scanner_session_count();
        st.flippers   = ble_scanner_session_flipper_count();
        st.airtags    = ble_scanner_session_airtag_count();
        st.skimmers   = ble_scanner_session_skimmer_count();
        st.freeHeapK  = ESP.getFreeHeap() / 1024;
        st.fw         = HUGINN_FW_VERSION;

        bool saveAuto = false, autoNow = true;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        if (s_autoReq) {
            s_autoReq = false;
            autoNow   = !td_ui_auto_rotate();
            td_ui_set_auto_rotate(autoNow, true);
            saveAuto  = true;
        }
        td_ui_render(s_fb, st);
        xSemaphoreGive(s_mutex);

        if (saveAuto) {
            Preferences prefs;
            if (prefs.begin("huginn", false)) {
                prefs.putBool("autopage", autoNow);
                prefs.end();
            }
            Serial.printf("[DISP] page auto-cycle %s (saved)\n", autoNow ? "on" : "off");
        }

        if (s_flipReq) {
            s_flipReq  = false;
            s_rotation = (s_rotation == 1) ? 3 : 1;
            lcdSetRotation();
            Preferences prefs;
            if (prefs.begin("huginn", false)) {
                prefs.putUChar("lcdrot", s_rotation);
                prefs.end();
            }
            Serial.printf("[DISP] screen flipped, rotation=%u (saved)\n", (unsigned)s_rotation);
        }
        lcdPush();

        // Fade the backlight in over the first half second.
        const uint32_t up = millis() - t0;
        if (up < 600) backlight((uint8_t)(up * TDONGLE_LCD_BRIGHTNESS / 600));
        else if (up < 700) backlight(TDONGLE_LCD_BRIGHTNESS);

        const uint32_t busy = micros() - start;
        busyUs += busy;
        if (busy > maxBusyUs) maxBusyUs = busy;
        frames++;
        if (millis() - lastReport >= TDONGLE_REPORT_MS) {
            Serial.printf("[DISP] %.1f fps, frame %u us avg / %u us max, worst gap %u ms, %u late\n",
                          frames * 1000.0f / (millis() - lastReport), (unsigned)(busyUs / frames),
                          (unsigned)maxBusyUs, (unsigned)(maxGapUs / 1000), (unsigned)late);
            frames = 0; busyUs = 0; maxGapUs = 0; maxBusyUs = 0; late = 0; lastReport = millis();
        }

        vTaskDelayUntil(&wake, pdMS_TO_TICKS(TDONGLE_FRAME_MS));
    }
}

void display_init() {
    if (s_started) return;
    s_started = true;

    s_fb = (uint16_t*)malloc((size_t)TD_UI_W * TD_UI_H * sizeof(uint16_t));
    if (!s_fb) {
        Serial.println("[DISP] framebuffer alloc FAILED — display disabled");
        return;
    }
    memset(s_fb, 0, (size_t)TD_UI_W * TD_UI_H * sizeof(uint16_t));

    // Backlight off (active LOW) until the first frame is on the glass.
    ledcAttach(TDONGLE_LCD_BL, 5000, 8);
    backlight(0);

    // Orientation (double-click) and page auto-cycle (triple-click) survive reboots.
    bool autoCycle = true;
    {
        Preferences prefs;
        if (prefs.begin("huginn", true)) {
            const uint8_t r = prefs.getUChar("lcdrot", TDONGLE_LCD_ROTATION);
            if (r == 1 || r == 3) s_rotation = r;
            autoCycle = prefs.getBool("autopage", true);
            prefs.end();
        }
    }

    lcdInit();
    lcdPush();          // black

    td_ui_init();
    td_ui_set_auto_rotate(autoCycle, false);
    s_mutex = xSemaphoreCreateMutex();

    // The task starts here (not in main) so the boot splash animates while the
    // radios are still initialising. The C5 is single-core: pin to core 0.
    xTaskCreatePinnedToCore(display_task, "display", DISPLAY_TASK_STACK, NULL, TDONGLE_TASK_PRIORITY, NULL, 0);
}

#endif // HUGINN_HAS_DISPLAY && HUGINN_BOARD_TDONGLE_C5
