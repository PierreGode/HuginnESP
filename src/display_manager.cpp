#include "display_manager.h"
#include "serial_cmd.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "scan_cycle.h"
#include "config.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// =====================================================================
//  LovyanGFX panel configuration for Waveshare ESP32-S3 Smart 86 Box
//  4" IPS 480×480 RGB interface + GT911 touch (I2C)
// =====================================================================
class LGFX_WaveshareBox : public lgfx::LGFX_Device {
    lgfx::Panel_RGB  _panel;
    lgfx::Bus_RGB    _bus;
    lgfx::Light_PWM  _light;
    lgfx::Touch_GT911 _touch;

public:
    LGFX_WaveshareBox() {
        // ---- Bus (RGB) ----
        {
            auto cfg = _bus.config();
            cfg.panel = &_panel;

            cfg.pin_d0  = GPIO_NUM_8;   // B0
            cfg.pin_d1  = GPIO_NUM_3;   // B1
            cfg.pin_d2  = GPIO_NUM_46;  // B2
            cfg.pin_d3  = GPIO_NUM_9;   // B3
            cfg.pin_d4  = GPIO_NUM_1;   // B4

            cfg.pin_d5  = GPIO_NUM_5;   // G0
            cfg.pin_d6  = GPIO_NUM_6;   // G1
            cfg.pin_d7  = GPIO_NUM_7;   // G2
            cfg.pin_d8  = GPIO_NUM_15;  // G3
            cfg.pin_d9  = GPIO_NUM_16;  // G4
            cfg.pin_d10 = GPIO_NUM_4;   // G5

            cfg.pin_d11 = GPIO_NUM_45;  // R0
            cfg.pin_d12 = GPIO_NUM_48;  // R1
            cfg.pin_d13 = GPIO_NUM_47;  // R2
            cfg.pin_d14 = GPIO_NUM_21;  // R3
            cfg.pin_d15 = GPIO_NUM_14;  // R4

            cfg.pin_henable = GPIO_NUM_40;
            cfg.pin_vsync   = GPIO_NUM_41;
            cfg.pin_hsync   = GPIO_NUM_39;
            cfg.pin_pclk    = GPIO_NUM_42;

            cfg.freq_write = 12000000;
            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 16;
            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 4;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 4;
            cfg.pclk_idle_high    = 1;

            _bus.config(cfg);
        }

        // ---- Panel ----
        {
            auto cfg = _panel.config();
            cfg.memory_width  = SCREEN_WIDTH;
            cfg.memory_height = SCREEN_HEIGHT;
            cfg.panel_width   = SCREEN_WIDTH;
            cfg.panel_height  = SCREEN_HEIGHT;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel.config(cfg);
        }

        _panel.setBus(&_bus);

        // ---- Backlight ----
        {
            auto cfg = _light.config();
            cfg.pin_bl = GPIO_NUM_2;
            cfg.invert = false;
            cfg.freq   = 1000;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }

        // ---- Touch (GT911) ----
        {
            auto cfg = _touch.config();
            cfg.x_min = 0;
            cfg.x_max = SCREEN_WIDTH - 1;
            cfg.y_min = 0;
            cfg.y_max = SCREEN_HEIGHT - 1;
            cfg.pin_int  = GPIO_NUM_NC;
            cfg.pin_rst  = GPIO_NUM_NC;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port = 0;
            cfg.i2c_addr = 0x5D;
            cfg.pin_sda  = GPIO_NUM_17;
            cfg.pin_scl  = GPIO_NUM_18;
            cfg.freq     = 400000;
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }

        setPanel(&_panel);
    }
};

static LGFX_WaveshareBox lcd;

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

// ---- Colors ----
static const uint32_t COL_BG        = 0x000000;
static const uint32_t COL_HEADER    = 0x1A1A2E;
static const uint32_t COL_TEXT      = 0xE0E0E0;
static const uint32_t COL_ACCENT    = 0x00FF88;
static const uint32_t COL_FLIPPER   = 0x4488FF;
static const uint32_t COL_AIRTAG    = 0xFF8800;
static const uint32_t COL_SKIMMER   = 0xFF2222;
static const uint32_t COL_PINEAPPLE = 0xFFCC00;
static const uint32_t COL_SPAM      = 0xAA44FF;
static const uint32_t COL_BTN       = 0x333355;
static const uint32_t COL_BTN_ACT   = 0x00AA66;

// ---- Button definitions ----
struct Button {
    int x, y, w, h;
    const char* label;
    ScanMode mode;
};

static const Button BUTTONS[] = {
    {  10, 400, 88, 36, "WiFi",  MODE_WIFI         },
    { 103, 400, 88, 36, "BLE",   MODE_BLE_ALL      },
    { 196, 400, 88, 36, "Flip",  MODE_BLE_FILTERED },
    { 289, 400, 88, 36, "Stop",  MODE_IDLE          },
    { 382, 400, 88, 36, "Auto",  MODE_AUTO_CYCLE    },
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
    lcd.fillRect(0, 0, SCREEN_WIDTH, 40, COL_HEADER);

    char buf[64];
    snprintf(buf, sizeof(buf), "WiFi:%d  BLE:%d  Mode:%s",
             wifi_scanner_count(), ble_scanner_count(),
             scanModeName(g_currentMode));
    lcd.setTextColor(COL_ACCENT, COL_HEADER);
    lcd.setTextSize(1);
    lcd.drawString(buf, 10, 12);

    char up[16];
    uptimeStr(up, sizeof(up));
    lcd.drawString(up, SCREEN_WIDTH - 80, 12);
}

// ---- Draw buttons ----
static void drawButtons() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        const Button& b = BUTTONS[i];
        bool active = (g_currentMode == b.mode);
        uint32_t col = active ? COL_BTN_ACT : COL_BTN;
        lcd.fillRoundRect(b.x, b.y, b.w, b.h, 6, col);
        lcd.setTextColor(COL_TEXT, col);
        lcd.drawCenterString(b.label, b.x + b.w / 2, b.y + 10);
    }
}

// ---- Draw alert panel ----
static void drawAlerts() {
    lcd.fillRect(0, 240, SCREEN_WIDTH, 155, COL_BG);
    lcd.setTextColor(COL_TEXT, COL_BG);
    lcd.drawString("-- Alerts --", 10, 245);

    if (s_alertMutex) xSemaphoreTake(s_alertMutex, portMAX_DELAY);
    int count = min(s_alertCount, MAX_ALERTS);
    for (int i = 0; i < count; i++) {
        int idx = (s_alertHead - count + i + MAX_ALERTS) % MAX_ALERTS;
        AlertEntry& a = s_alerts[idx];
        uint32_t col = COL_TEXT;
        const char* icon = "";
        if (strcmp(a.type, "flipper") == 0)        { col = COL_FLIPPER;   icon = "[F] "; }
        else if (strcmp(a.type, "airtag") == 0)    { col = COL_AIRTAG;    icon = "[A] "; }
        else if (strcmp(a.type, "skimmer") == 0)   { col = COL_SKIMMER;   icon = "[S] "; }
        else if (strcmp(a.type, "pineapple") == 0) { col = COL_PINEAPPLE; icon = "[P] "; }
        else if (strcmp(a.type, "spam") == 0)      { col = COL_SPAM;      icon = "[!] "; }

        char line[64];
        snprintf(line, sizeof(line), "%s%s %ddBm", icon, a.mac, a.rssi);
        lcd.setTextColor(col, COL_BG);
        lcd.drawString(line, 10, 260 + i * 18);
    }
    if (s_alertMutex) xSemaphoreGive(s_alertMutex);
}

// ---- Touch handling ----
static void handleTouch() {
    lgfx::touch_point_t tp;
    if (!lcd.getTouch(&tp)) return;

    for (int i = 0; i < NUM_BUTTONS; i++) {
        const Button& b = BUTTONS[i];
        if (tp.x >= b.x && tp.x <= b.x + b.w &&
            tp.y >= b.y && tp.y <= b.y + b.h) {

            switch (b.mode) {
                case MODE_WIFI:
                    g_manualOverride = true;
                    ble_scanner_stop();
                    g_currentMode = MODE_WIFI;
                    wifi_scanner_start();
                    break;
                case MODE_BLE_ALL:
                    g_manualOverride = true;
                    wifi_scanner_stop();
                    g_currentMode = MODE_BLE_ALL;
                    ble_scanner_start(BLE_MODE_ALL);
                    break;
                case MODE_BLE_FILTERED:
                    g_manualOverride = true;
                    wifi_scanner_stop();
                    g_currentMode = MODE_BLE_FILTERED;
                    ble_scanner_start(BLE_MODE_FILTERED);
                    break;
                case MODE_IDLE:
                    g_manualOverride = false;
                    wifi_scanner_stop();
                    ble_scanner_stop();
                    g_currentMode = MODE_AUTO_CYCLE;
                    scan_cycle_resume();
                    break;
                case MODE_AUTO_CYCLE:
                    g_manualOverride = false;
                    g_currentMode = MODE_AUTO_CYCLE;
                    scan_cycle_resume();
                    break;
                default:
                    break;
            }
            // Debounce
            delay(200);
            break;
        }
    }
}

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
    lcd.init();
    lcd.setRotation(0);
    lcd.setBrightness(200);
    lcd.fillScreen(COL_BG);
    lcd.setFont(&fonts::Font2);

    // Splash
    lcd.setTextColor(COL_ACCENT, COL_BG);
    lcd.drawCenterString("Ragnar Scanner", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 20);
    lcd.drawCenterString("Initializing...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 10);
    delay(1500);
    lcd.fillScreen(COL_BG);
}

void display_task(void* param) {
    for (;;) {
        drawStatusBar();
        drawButtons();
        drawAlerts();
        handleTouch();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
