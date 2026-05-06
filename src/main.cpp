// =====================================================================
//  Ragnar ESP32-S3 Scanner Firmware
//  Board: Waveshare ESP32-S3 Smart 86 Box (4" 480×480 RGB touch)
//
//  Scans WiFi & BLE, detects Flipper Zero / AirTag / Skimmer /
//  Evil-Twin / BLE spam.  Outputs data over USB serial (115200) in
//  formats compatible with Ragnar's wardriving.py parser.
// =====================================================================

#include <Arduino.h>
#include "config.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "serial_cmd.h"
#include "scan_cycle.h"
#include "display_manager.h"

// FreeRTOS task handles
static TaskHandle_t s_cycleTask   = nullptr;
static TaskHandle_t s_displayTask = nullptr;

// Serial command polling task
static void serialTask(void* param) {
    for (;;) {
        serial_cmd_poll();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);

    // Initialize subsystems
    wifi_scanner_init();
    ble_scanner_init();
    serial_cmd_init();
    display_init();
    scan_cycle_init();

    // Launch FreeRTOS tasks
    xTaskCreatePinnedToCore(scan_cycle_task, "cycle",   CYCLE_TASK_STACK,   nullptr, 1, &s_cycleTask,   0);
    xTaskCreatePinnedToCore(display_task,    "display", DISPLAY_TASK_STACK, nullptr, 1, &s_displayTask, 1);
    xTaskCreatePinnedToCore(serialTask,      "serial",  SERIAL_TASK_STACK,  nullptr, 2, nullptr,        0);
}

void loop() {
    // All work handled by FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}
