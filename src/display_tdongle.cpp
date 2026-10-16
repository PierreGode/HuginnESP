#include "config.h"

#if HUGINN_HAS_DISPLAY && HUGINN_BOARD_TDONGLE_C5
#include "display_manager.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "zigbee_scanner.h"
#include "serial_cmd.h"
#include <SPI.h>
#include <Adafruit_ST7735.h>

// Pinout and active-low backlight from LilyGO's Factory/pin_config.h.
static constexpr int LCD_MOSI = 2, LCD_SCK = 6, LCD_CS = 10;
static constexpr int LCD_DC = 3, LCD_RST = 1, LCD_BL = 0, SD_CS = 23;
static Adafruit_ST7735 screen(LCD_CS, LCD_DC, LCD_MOSI, LCD_SCK, LCD_RST);
static SemaphoreHandle_t alertMutex = nullptr;
static char lastAlert[26] = "No alerts";
static uint32_t alertAt = 0;
static bool ready = false;

void display_add_alert(const char* type, const char*, int rssi) {
    if (!alertMutex || xSemaphoreTake(alertMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
    snprintf(lastAlert, sizeof(lastAlert), "%.15s %ddBm", type, rssi);
    alertAt = millis();
    xSemaphoreGive(alertMutex);
}

void display_init() {
    alertMutex = xSemaphoreCreateMutex();
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);  // SD shares SPI; keep it deselected.
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);
    Serial.println("[DISP] T-Dongle ST7735 software SPI init");
    // Plugin-FPC profile matches LilyGO's inverted panel and 26/1 RAM offsets.
    screen.initR(INITR_MINI160x80_PLUGIN);
    screen.setRotation(1);  // MY|MV|BGR, matching LilyGO's landscape example.
    screen.setTextWrap(false);
    screen.fillScreen(ST77XX_BLACK);
    digitalWrite(LCD_BL, LOW);
    ready = true;
}

void display_task(void*) {
    if (!ready) { vTaskDelete(nullptr); return; }
    Serial.println("[DISP] T-Dongle display task started");
    for (;;) {
        char rows[6][27];
        snprintf(rows[0], sizeof(rows[0]), "HUGINN  %s", scanModeName(g_currentMode));
        snprintf(rows[1], sizeof(rows[1]), "WiFi %4d   BLE %4d", wifi_scanner_session_count(), ble_scanner_session_count());
        snprintf(rows[2], sizeof(rows[2]), "ZB %4d  Flip %3d", zigbee_scanner_session_count(), ble_scanner_session_flipper_count());
        snprintf(rows[3], sizeof(rows[3]), "Tags %3d  Suspect %3d", ble_scanner_session_airtag_count(), ble_scanner_session_skimmer_count());
        snprintf(rows[4], sizeof(rows[4]), "Up %lum  Heap %luk", millis() / 60000UL, (unsigned long)(ESP.getFreeHeap() / 1024));
        strlcpy(rows[5], "No recent alerts", sizeof(rows[5]));
        if (alertMutex && xSemaphoreTake(alertMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (millis() - alertAt < 10000 && alertAt) strlcpy(rows[5], lastAlert, sizeof(rows[5]));
            xSemaphoreGive(alertMutex);
        }
        // A small text-only update avoids allocating a framebuffer on the C5.
        screen.setTextSize(1);
        for (int i = 0; i < 6; ++i) {
            screen.fillRect(0, i * 13, 160, 12, ST77XX_BLACK);
            screen.setTextColor(i == 0 ? ST77XX_CYAN : (i == 5 ? ST77XX_YELLOW : ST77XX_WHITE));
            screen.setCursor(1, i * 13 + 2);
            screen.print(rows[i]);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif
