#ifndef TDONGLE_UI_H
#define TDONGLE_UI_H

// =====================================================================
//  Animated 160x80 UI for the LilyGO T-Dongle-C5 (ST7735 0.96" IPS).
//
//  Pure, portable C++ — no Arduino / ESP-IDF dependencies — so the exact
//  same renderer runs on the device (src/display_tdongle.cpp) and in the
//  host preview (sim/tdongle_ui_sim.cpp). It draws every frame from
//  scratch into a caller-supplied RGB565 framebuffer.
//
//  NOT thread-safe: the caller serialises td_ui_note_*() / td_ui_alert()
//  against td_ui_render() (the device wraps them in one mutex).
// =====================================================================

#include <stdint.h>

#define TD_UI_W 160
#define TD_UI_H 80

// Wi-Fi security classes for td_ui_note_wifi().
enum TdUiSec : uint8_t {
    TD_SEC_OPEN = 0,
    TD_SEC_WEP  = 1,
    TD_SEC_WPA  = 2,   // WPA / WPA2 / mixed
    TD_SEC_WPA3 = 3,
};

// BLE flags for td_ui_note_ble().
#define TD_BLE_FLIPPER 0x01
#define TD_BLE_AIRTAG  0x02   // any separated Find My tracker (AirTag / 3rd-party tag / AirPods)
#define TD_BLE_SKIMMER 0x04

// Slow-changing status sampled by the caller once per frame.
struct TdUiStatus {
    uint32_t    nowMs;        // monotonic milliseconds
    const char* mode;         // scan mode name ("wardrive", "skimmer", ...)
    int         sessWifi;     // unique BSSIDs since boot
    int         sessBle;      // unique BLE MACs since boot
    int         sessZigbee;   // unique 802.15.4 devices since boot
    int         flippers;     // unique Flipper Zeros since boot
    int         airtags;      // unique separated Find My trackers since boot
    int         skimmers;     // unique suspected skimmers since boot
    uint32_t    freeHeapK;    // free heap, KiB
    const char* fw;           // firmware version string
};

void td_ui_init();

// Feed one sighting. `hash` identifies the device (e.g. a hash of its MAC).
void td_ui_note_wifi(uint32_t nowMs, uint32_t hash, const char* ssid,
                     int rssi, int channel, TdUiSec sec);
void td_ui_note_ble(uint32_t nowMs, uint32_t hash, const char* mac,
                    int rssi, uint8_t flags);
// 802.15.4 device; `label` is shown in the target list (e.g. "thread 0x1A2B").
void td_ui_note_zigbee(uint32_t nowMs, uint32_t hash, const char* label,
                       int rssi, int channel);

// Full-screen alert takeover (e.g. "FLIPPER", "PINEAPPLE").
void td_ui_alert(uint32_t nowMs, const char* type, const char* detail, int rssi);

// Advance to the next page now (button short-press).
void td_ui_next_page();

// Enable / disable the automatic page rotation. With it off the current page
// stays until td_ui_next_page(). `announce` shows a short on-screen notice.
void td_ui_set_auto_rotate(bool enabled, bool announce);
bool td_ui_auto_rotate();

// Render one frame into fb[TD_UI_W * TD_UI_H] (native-endian RGB565).
void td_ui_render(uint16_t* fb, const TdUiStatus& st);

#endif // TDONGLE_UI_H
