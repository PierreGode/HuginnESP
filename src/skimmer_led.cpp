#include "skimmer_led.h"

#if HUGINN_HAS_SKIMMER_LED

// Shared between the BLE task (writer, via skimmer_led_notify) and the LED
// task (reader). All three are 32-bit aligned scalars, so plain volatile
// access is atomic on the ESP32 — no tearing, no mutex needed. A benign race
// where the reader pairs a fresh timestamp with a stale RSSI/type only nudges
// the blink for one cycle, which is harmless.
static volatile int32_t  s_lastRssi   = -100;
static volatile uint32_t s_lastSeenMs = 0;
static volatile int32_t  s_lastType   = SKIMMER_LED_SKIMMER;
static bool s_started = false;

// One-shot confirmation flash request (mode-switch feedback). The button task
// writes these; the LED task plays and clears the pending count.
static volatile uint8_t  s_flashR = 0, s_flashG = 0, s_flashB = 0;
static volatile int32_t  s_flashPending = 0;

// Map an RSSI to a blink half-period (ms): closer (stronger signal, i.e. RSSI
// nearer 0) blinks faster. RSSI is clamped to the configured NEAR..FAR window,
// then linearly interpolated across FAST..SLOW.
static uint32_t rssiToHalfPeriod(int rssi) {
    if (rssi > SKIMMER_LED_RSSI_NEAR) rssi = SKIMMER_LED_RSSI_NEAR;
    if (rssi < SKIMMER_LED_RSSI_FAR)  rssi = SKIMMER_LED_RSSI_FAR;

    const long span = (long)SKIMMER_LED_RSSI_NEAR - (long)SKIMMER_LED_RSSI_FAR; // > 0
    const long pos  = (long)rssi - (long)SKIMMER_LED_RSSI_FAR;                  // 0..span
    return (uint32_t)((long)SKIMMER_LED_SLOW_MS -
        (pos * ((long)SKIMMER_LED_SLOW_MS - (long)SKIMMER_LED_FAST_MS)) / span);
}

// One LED-write primitive for both supported LED types.
#if HUGINN_LED_APA102
// APA102 (e.g. LilyGO T-Dongle-C5): two-wire clock + data, bit-banged. One
// LED, written a few times a second, so speed is irrelevant.
static void apa102Byte(uint8_t v) {
    for (int i = 7; i >= 0; i--) {
        digitalWrite(APA102_DATA_PIN, (v >> i) & 1);
        digitalWrite(APA102_CLOCK_PIN, HIGH);
        digitalWrite(APA102_CLOCK_PIN, LOW);
    }
}
static void ledWrite(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < 4; i++) apa102Byte(0x00);            // start frame
    apa102Byte(0xE0 | (APA102_GLOBAL_BRIGHTNESS & 0x1F));    // LED frame header
    apa102Byte(b);
    apa102Byte(g);
    apa102Byte(r);
    for (int i = 0; i < 4; i++) apa102Byte(0xFF);            // end frame
}
static void ledHwInit() {
    pinMode(APA102_DATA_PIN, OUTPUT);
    pinMode(APA102_CLOCK_PIN, OUTPUT);
    digitalWrite(APA102_CLOCK_PIN, LOW);
}
#else
static inline void ledWrite(uint8_t r, uint8_t g, uint8_t b) {
    rgbLedWrite(SKIMMER_LED_PIN, r, g, b);
}
static inline void ledHwInit() {}
#endif

static inline void ledOff() {
    ledWrite(0, 0, 0);
}

// Show one phase of the alternating blink. Phase 0 is the type's signature
// color (red for skimmer, blue for Flipper); phase 1 is white for both.
static void ledPhase(bool whitePhase, int type) {
    const uint8_t b = SKIMMER_LED_BRIGHTNESS;
    uint8_t r, g, bl;
    if (whitePhase) {
        r = g = bl = b;                                   // white
    } else if (type == SKIMMER_LED_FLIPPER) {
        r = 0; g = 0; bl = b;                             // blue
    } else {
        r = b; g = 0; bl = 0;                             // red (skimmer)
    }
    ledWrite(r, g, bl);
}

static void skimmer_led_task(void*) {
    bool whitePhase = false;
    ledOff();

    for (;;) {
        // Pending confirmation flash takes priority and plays synchronously.
        if (s_flashPending > 0) {
            const int     n = s_flashPending;
            const uint8_t r = s_flashR, g = s_flashG, b = s_flashB;
            s_flashPending = 0;
            for (int i = 0; i < n; i++) {
                ledWrite(r, g, b);
                vTaskDelay(pdMS_TO_TICKS(150));
                ledWrite(0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(150));
            }
            whitePhase = false;
            continue;
        }

        const uint32_t now = millis();
        const uint32_t age = now - (uint32_t)s_lastSeenMs;

        if (s_lastSeenMs != 0 && age <= SKIMMER_LED_HOLD_MS) {
            // A flagged device is (recently) in range — alternate the two
            // colors at the proximity rate.
            whitePhase = !whitePhase;
            ledPhase(whitePhase, (int)s_lastType);
            vTaskDelay(pdMS_TO_TICKS(rssiToHalfPeriod((int)s_lastRssi)));
        } else {
            // Nothing nearby — make sure the LED is off and idle-poll.
            ledOff();
            whitePhase = false;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void skimmer_led_init() {
    if (s_started) return;
    s_started = true;
    ledHwInit();
    ledOff();
    // Pinned to core 0 so it coexists with the scan cycle on single-core C5.
    xTaskCreatePinnedToCore(skimmer_led_task, "skimmer_led",
                            SKIMMER_LED_TASK_STACK, NULL, 1, NULL, 0);
}

void skimmer_led_notify(int rssi, SkimmerLedAlert type) {
    s_lastRssi   = rssi;
    s_lastType   = (int32_t)type;
    s_lastSeenMs = millis();
}

void skimmer_led_flash(uint8_t r, uint8_t g, uint8_t b, int count) {
    s_flashR = r;
    s_flashG = g;
    s_flashB = b;
    s_flashPending = count;
}

#endif // HUGINN_HAS_SKIMMER_LED
