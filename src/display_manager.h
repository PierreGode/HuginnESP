#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>

// Two display back-ends share this interface:
//   * display_manager.cpp  — Waveshare ESP32-S3-Touch-LCD-4B (480x480 RGB)
//   * display_tdongle.cpp  — LilyGO T-Dongle-C5 (160x80 ST7735, animated UI)
// Everything below compiles to a no-op on headless boards, so callers never
// need their own #if guards.

#if HUGINN_HAS_DISPLAY

void display_init();
void display_task(void* param);

// Called from other modules to push alert events
void display_add_alert(const char* type, const char* mac, int rssi);

#else

static inline void display_add_alert(const char*, const char*, int) {}

#endif // HUGINN_HAS_DISPLAY

#if HUGINN_HAS_DISPLAY && HUGINN_BOARD_TDONGLE_C5

// Live sighting feed for the animated UI (radar blips, spectrum, target list).
// Safe to call from any task.
void display_note_wifi(const uint8_t bssid[6], const char* ssid, int rssi,
                       int channel, const char* security);
void display_note_ble(const char* mac, int rssi,
                      bool flipper, bool airtag, bool skimmer);

// Advance to the next UI page (button short-press).
void display_next_page();

// Rotate the picture 180 degrees (button double-click). Remembered across boots.
void display_flip();

// Stop / resume the automatic page rotation (button triple-click). Remembered
// across boots.
void display_toggle_auto_cycle();

#else

static inline void display_note_wifi(const uint8_t*, const char*, int, int, const char*) {}
static inline void display_note_ble(const char*, int, bool, bool, bool) {}
static inline void display_next_page() {}
static inline void display_flip() {}
static inline void display_toggle_auto_cycle() {}

#endif

#endif // DISPLAY_MANAGER_H
