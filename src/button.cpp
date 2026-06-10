#include "button.h"

#if HUGINN_HAS_MODE_BUTTON

#include "serial_cmd.h"     // ScanMode, g_currentMode, g_manualOverride
#include "skimmer_led.h"    // confirmation flashes

// Brightness for the confirmation flashes. Mirrors the LED brightness when the
// LED feature is compiled in; falls back to a sane value otherwise.
#ifndef SKIMMER_LED_BRIGHTNESS
#define SKIMMER_LED_BRIGHTNESS 40
#endif

static bool s_started = false;

// Toggle between wardrive and skimmer-only. The scan-cycle task owns all
// scanner start/stop transitions; here we only flip the shared mode globals
// and flash the confirmation. Skimmer takes the "else" so any non-skimmer mode
// (wardrive, auto, etc.) switches into skimmer on first press.
static void toggleMode() {
    if (g_currentMode == MODE_SKIMMER) {
        g_manualOverride = true;
        g_currentMode    = MODE_WARDRIVE;
        skimmer_led_flash(0, SKIMMER_LED_BRIGHTNESS, 0, 3);                    // green
    } else {
        g_manualOverride = true;
        g_currentMode    = MODE_SKIMMER;
        skimmer_led_flash(SKIMMER_LED_BRIGHTNESS, 0, SKIMMER_LED_BRIGHTNESS, 3); // purple
    }
}

static void button_task(void*) {
    pinMode(MODE_BTN_PIN, INPUT_PULLUP);

    bool     wasDown    = false;
    bool     fired      = false;   // one toggle per hold
    uint32_t pressStart = 0;

    for (;;) {
        const bool     down = (digitalRead(MODE_BTN_PIN) == LOW); // active-low
        const uint32_t now  = millis();

        if (down && !wasDown) {            // press begins
            pressStart = now;
            fired = false;
        }
        if (down && !fired && (now - pressStart) >= MODE_BTN_LONGPRESS_MS) {
            toggleMode();                  // long-press reached
            fired = true;
        }
        wasDown = down;

        vTaskDelay(pdMS_TO_TICKS(MODE_BTN_POLL_MS));
    }
}

void button_init() {
    if (s_started) return;
    s_started = true;
    xTaskCreatePinnedToCore(button_task, "mode_btn",
                            MODE_BTN_TASK_STACK, NULL, 1, NULL, 0);
}

#endif // HUGINN_HAS_MODE_BUTTON
