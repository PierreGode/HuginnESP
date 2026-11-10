#include "button.h"

#if HUGINN_HAS_MODE_BUTTON

#include "serial_cmd.h"     // ScanMode, g_currentMode, g_manualOverride
#include "skimmer_led.h"    // confirmation flashes
#include "display_manager.h" // short-press page flip (no-op without the animated UI)

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

    bool     wasDown      = false;
    bool     fired        = false;   // one toggle per hold
    uint32_t pressStart   = 0;
    int      clicks       = 0;       // short clicks collected so far
    uint32_t clickAt      = 0;       // when the latest one was released

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
        if (down && !fired && (now - pressStart) >= MODE_BTN_LONGPRESS_MS / 2) {
            clicks = 0;                    // turning into a long press, not a click
        }
        if (!down && wasDown && !fired && (now - pressStart) >= 2 * MODE_BTN_POLL_MS
                && (now - pressStart) < MODE_BTN_LONGPRESS_MS / 2) {
            clicks++;                      // short press released
            clickAt = now;
        }
        // Display boards: act once the multi-click window has passed (or at
        // once on the third click) — 1 = next page, 2 = flip the screen 180
        // deg, 3 = stop / resume the page auto-cycle.
        if (clicks > 0 && !down
                && (clicks >= 3 || (now - clickAt) > MODE_BTN_DOUBLECLICK_MS)) {
            if (clicks >= 3)      display_toggle_auto_cycle();
            else if (clicks == 2) display_flip();
            else                  display_next_page();
            clicks = 0;
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
