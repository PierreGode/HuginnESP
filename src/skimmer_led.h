#ifndef SKIMMER_LED_H
#define SKIMMER_LED_H

#include <Arduino.h>
#include "config.h"

// Optional proximity LED feedback for skimmer detection.
//
// When enabled (-DHUGINN_HAS_SKIMMER_LED=1) the board's addressable "white"
// LED (e.g. the ESP32-C5 DevKitC RGB_BUILTIN) blinks whenever a potential
// skimmer is in range, and blinks faster the closer it is — stronger RSSI
// means a shorter half-period. The LED stops on its own once no skimmer has
// been seen for SKIMMER_LED_HOLD_MS.

#if HUGINN_HAS_SKIMMER_LED

// Start the LED driver task. Safe no-op if already started.
void skimmer_led_init();

// Report a fresh skimmer sighting and its RSSI (called from the BLE callback).
// Resets the hold timer; the blink rate tracks the most recent RSSI.
void skimmer_led_notify(int rssi);

#else

// Compiled-out no-ops so callers don't need their own #if guards.
static inline void skimmer_led_init() {}
static inline void skimmer_led_notify(int) {}

#endif // HUGINN_HAS_SKIMMER_LED

#endif // SKIMMER_LED_H
