#ifndef CONFIG_H
#define CONFIG_H

// ----- Firmware version -----
#define HUGINN_FW_VERSION "2.0"

// ----- Task stack sizes -----
#define CYCLE_TASK_STACK   8192

// ----- Skimmer suspicious names (defaults; runtime list lives in runtime_config) -----
static const char* SKIMMER_NAMES_DEFAULT[] = {
    "HC-05", "HC-06", "HC-08",
    "HC05", "HC06", "HC08",
    "BT05", "BT06",
    "CC41A", "CC41",
    "JDY-30", "JDY-31", "JDY-33",
    "JDY30", "JDY31", "JDY33",
    "SPP-CA",
    "LINVOR",
    "MLT-BT05",
    nullptr
};

// ----- Proximity alert LED (optional — enabled by -DHUGINN_HAS_SKIMMER_LED=1) -----
#if HUGINN_HAS_SKIMMER_LED
#ifndef SKIMMER_LED_PIN
#ifdef RGB_BUILTIN
#define SKIMMER_LED_PIN        RGB_BUILTIN
#else
#define SKIMMER_LED_PIN        LED_BUILTIN
#endif
#endif
#ifndef SKIMMER_LED_BRIGHTNESS
#define SKIMMER_LED_BRIGHTNESS 40      // 0-255 white level when lit (these LEDs are bright)
#endif
// RSSI is negative; closer ≈ nearer 0 (e.g. -45), farther ≈ more negative (-95).
#define SKIMMER_LED_RSSI_NEAR  -45     // at/above this → fastest blink
#define SKIMMER_LED_RSSI_FAR   -95     // at/below this → slowest blink
#define SKIMMER_LED_FAST_MS     70     // blink half-period at closest range
#define SKIMMER_LED_SLOW_MS   1000     // blink half-period at farthest range
#define SKIMMER_LED_HOLD_MS  10000     // keep blinking this long after the last sighting
#define SKIMMER_LED_TASK_STACK 2048
#endif

#endif // CONFIG_H
