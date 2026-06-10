#ifndef SKIMMER_LED_H
#define SKIMMER_LED_H

#include <Arduino.h>
#include "config.h"

// Optional proximity alert LED, driven by the board's addressable RGB LED
// (e.g. the ESP32-C5 WIFI6-KIT WS2812B on RGB_BUILTIN).
//
// When enabled (-DHUGINN_HAS_SKIMMER_LED=1) the LED blinks while a flagged
// device is in range and blinks faster the closer it is (stronger RSSI means
// a shorter half-period). The blink colors identify the alert type:
//   * Skimmer  → alternates RED   <-> white
//   * Flipper  → alternates BLUE  <-> white
// The LED turns itself off once nothing has been seen for SKIMMER_LED_HOLD_MS.

// Alert type carried into the LED. Defined unconditionally so callers compile
// the same whether or not the feature is enabled.
enum SkimmerLedAlert {
    SKIMMER_LED_SKIMMER = 0,
    SKIMMER_LED_FLIPPER = 1,
};

#if HUGINN_HAS_SKIMMER_LED

// Start the LED driver task. Safe no-op if already started.
void skimmer_led_init();

// Report a fresh sighting and its RSSI (called from the BLE callback). Resets
// the hold timer; the blink rate tracks the most recent RSSI and the colors
// track the most recent alert type.
void skimmer_led_notify(int rssi, SkimmerLedAlert type);

#else

// Compiled-out no-ops so callers don't need their own #if guards.
static inline void skimmer_led_init() {}
static inline void skimmer_led_notify(int, SkimmerLedAlert) {}

#endif // HUGINN_HAS_SKIMMER_LED

#endif // SKIMMER_LED_H
