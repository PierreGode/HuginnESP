#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>
#include "config.h"

// Optional physical mode button.
//
// When enabled (-DHUGINN_HAS_MODE_BUTTON=1) a long-press on the onboard BOOT
// button toggles the scan mode between wardrive and skimmer-only. The proximity
// LED confirms the switch (3 purple blinks = skimmer, 3 green = wardrive).

#if HUGINN_HAS_MODE_BUTTON

// Start the button polling task. Safe no-op if already started.
void button_init();

#else

static inline void button_init() {}

#endif // HUGINN_HAS_MODE_BUTTON

#endif // BUTTON_H
