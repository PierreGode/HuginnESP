// =====================================================================
//  HuginnESP — Ragnar ESP32-S3 Scanner Firmware
//  Board: Waveshare ESP32-S3-Touch-LCD-4B (N16R8, 4" 480×480 RGB touch)
//
//  Scans WiFi & BLE, detects Flipper Zero / AirTag / Skimmer /
//  Evil-Twin / BLE spam.  Outputs data over USB serial (115200) in
//  formats compatible with Ragnar's wardriving.py parser.
// =====================================================================

#include <Arduino.h>
#include "config.h"
#include "display_manager.h"
#include "ble_scanner.h"
#include "wifi_scanner.h"
#include "serial_cmd.h"
#include "scan_cycle.h"

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(2000);
    Serial.println("[BOOT] HuginnESP starting...");
    Serial.printf("[BOOT] Free heap: %u\n", ESP.getFreeHeap());
    Serial.printf("[BOOT] PSRAM: %u\n", ESP.getPsramSize());
    Serial.flush();

    // WiFi MUST init before display — WiFi ISR conflicts with RGB DMA cache
    Serial.println("[BOOT] Init WiFi...");
    wifi_scanner_init();
    Serial.println("[BOOT] WiFi OK");

    // Display
    Serial.println("[BOOT] Init display...");
    display_init();
    Serial.println("[BOOT] Display OK");

    // BLE
    Serial.println("[BOOT] Init BLE...");
    ble_scanner_init();
    Serial.println("[BOOT] BLE OK");

    // Serial command parser
    serial_cmd_init();

    // Scan cycle
    scan_cycle_init();

    Serial.printf("[BOOT] Free heap after init: %u\n", ESP.getFreeHeap());
    Serial.flush();

    // Start FreeRTOS tasks
    xTaskCreatePinnedToCore(scan_cycle_task, "scan_cycle", CYCLE_TASK_STACK, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(display_task,    "display",    DISPLAY_TASK_STACK, NULL, 1, NULL, 1);

    Serial.println("[BOOT] All tasks started — entering main loop");
}

void loop() {
    serial_cmd_poll();
    delay(10);
}
