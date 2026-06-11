// =====================================================================
//  SkimGuard C5 — BLE skimmer detector firmware
//
//  Runtime model:
//  - BLE scan runs continuously (100% duty) in skimmer-focused mode.
//  - No wardriving and no serial command/protocol output.
//  - Onboard RGB LED indicates proximity to suspicious devices.
// =====================================================================

#include <Arduino.h>
#include "config.h"
#include "ble_scanner.h"
#include "scan_cycle.h"
#include "runtime_config.h"
#include "skimmer_led.h"

void setup() {
    delay(250);

    runtime_config_init();
    ble_scanner_init();

    // Local-only alert path on the device.
    skimmer_led_init();
    scan_cycle_init();

    // Continuous BLE scan task. On C5 this runs on the single available core.
    xTaskCreatePinnedToCore(scan_cycle_task, "scan_cycle", CYCLE_TASK_STACK, NULL, 1, NULL, 0);
}

void loop() {
    delay(50);
}
