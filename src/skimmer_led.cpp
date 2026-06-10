#include "skimmer_led.h"

#if HUGINN_HAS_SKIMMER_LED

// Shared between the BLE task (writer, via skimmer_led_notify) and the LED
// task (reader). Both are 32-bit aligned scalars, so plain volatile access is
// atomic on the ESP32 — no tearing, no mutex needed. A benign race where the
// reader pairs a fresh timestamp with a stale RSSI only nudges the blink rate
// for one cycle, which is harmless.
static volatile int32_t  s_lastRssi   = -100;
static volatile uint32_t s_lastSeenMs = 0;
static bool s_started = false;

// Map a skimmer RSSI to a blink half-period (ms): closer (stronger signal,
// i.e. RSSI nearer 0) blinks faster. RSSI is clamped to the configured
// NEAR..FAR window, then linearly interpolated across FAST..SLOW.
static uint32_t rssiToHalfPeriod(int rssi) {
    if (rssi > SKIMMER_LED_RSSI_NEAR) rssi = SKIMMER_LED_RSSI_NEAR;
    if (rssi < SKIMMER_LED_RSSI_FAR)  rssi = SKIMMER_LED_RSSI_FAR;

    const long span = (long)SKIMMER_LED_RSSI_NEAR - (long)SKIMMER_LED_RSSI_FAR; // > 0
    const long pos  = (long)rssi - (long)SKIMMER_LED_RSSI_FAR;                  // 0..span
    return (uint32_t)((long)SKIMMER_LED_SLOW_MS -
        (pos * ((long)SKIMMER_LED_SLOW_MS - (long)SKIMMER_LED_FAST_MS)) / span);
}

// Drive the addressable RGB LED white (or off). rgbLedWrite handles the RMT
// setup internally on first call.
static inline void ledWrite(bool on) {
    const uint8_t v = on ? SKIMMER_LED_BRIGHTNESS : 0;
    rgbLedWrite(SKIMMER_LED_PIN, v, v, v);
}

static void skimmer_led_task(void*) {
    bool ledOn = false;
    ledWrite(false);

    for (;;) {
        const uint32_t now = millis();
        const uint32_t age = now - (uint32_t)s_lastSeenMs;

        if (s_lastSeenMs != 0 && age <= SKIMMER_LED_HOLD_MS) {
            // A skimmer is (recently) in range — toggle at the proximity rate.
            ledOn = !ledOn;
            ledWrite(ledOn);
            vTaskDelay(pdMS_TO_TICKS(rssiToHalfPeriod((int)s_lastRssi)));
        } else {
            // Nothing nearby — make sure the LED is off and idle-poll.
            if (ledOn) { ledOn = false; ledWrite(false); }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void skimmer_led_init() {
    if (s_started) return;
    s_started = true;
    ledWrite(false);
    // Pinned to core 0 so it coexists with the scan cycle on single-core C5.
    xTaskCreatePinnedToCore(skimmer_led_task, "skimmer_led",
                            SKIMMER_LED_TASK_STACK, NULL, 1, NULL, 0);
}

void skimmer_led_notify(int rssi) {
    s_lastRssi   = rssi;
    s_lastSeenMs = millis();
}

#endif // HUGINN_HAS_SKIMMER_LED
