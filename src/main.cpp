// =====================================================================
//  HuginnESP — Multi-board ESP32 scanner firmware
//
//  Supported boards (selected via PlatformIO env / build flags):
//    HUGINN_BOARD_S3 + HUGINN_HAS_DISPLAY=1
//        Waveshare ESP32-S3-Touch-LCD-4B (N16R8, 4" 480x480 RGB touch)
//    HUGINN_BOARD_C5 + HUGINN_HAS_DISPLAY=0
//        Waveshare ESP32-C5-WIFI6-KIT (16MB/8MB PSRAM, dual-band Wi-Fi 6, no display)
//
//  Scans Wi-Fi & BLE, detects Flipper Zero / AirTag / Skimmer /
//  Evil-Twin / BLE spam. Outputs over USB serial (115200) in
//  formats compatible with Ragnar's wardriving.py parser.
// =====================================================================

#include <Arduino.h>
#include "config.h"
#if HUGINN_HAS_DISPLAY
#include "display_manager.h"
#endif
#include "ble_scanner.h"
#include "wifi_scanner.h"
#include "serial_cmd.h"
#include "scan_cycle.h"

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(2000);
    Serial.println("[BOOT] HuginnESP starting...");
#if HUGINN_BOARD_S3
    Serial.println("[BOOT] Board: ESP32-S3-Touch-LCD-4B (display)");
#elif HUGINN_BOARD_C5
    Serial.println("[BOOT] Board: ESP32-C5-WIFI6-KIT (headless, dual-band Wi-Fi 6)");
#else
    Serial.println("[BOOT] Board: unknown");
#endif
    Serial.printf("[BOOT] Free heap: %u\n", ESP.getFreeHeap());
    Serial.printf("[BOOT] PSRAM: %u\n", ESP.getPsramSize());
    Serial.flush();

    // WiFi MUST init before display — WiFi ISR conflicts with RGB DMA cache
    Serial.println("[BOOT] Init WiFi...");
    wifi_scanner_init();
    Serial.println("[BOOT] WiFi OK");

#if HUGINN_HAS_DISPLAY
    Serial.println("[BOOT] Init display...");
    display_init();
    Serial.println("[BOOT] Display OK");
#endif

    Serial.println("[BOOT] Init BLE...");
    ble_scanner_init();
    Serial.println("[BOOT] BLE OK");

    serial_cmd_init();
    scan_cycle_init();

    Serial.printf("[BOOT] Free heap after init: %u\n", ESP.getFreeHeap());
    Serial.flush();

    // Pin scan cycle to core 0 (works on dual-core S3 and single-core C5).
    xTaskCreatePinnedToCore(scan_cycle_task, "scan_cycle", CYCLE_TASK_STACK, NULL, 1, NULL, 0);
#if HUGINN_HAS_DISPLAY
    // Display rendering on core 1 (S3 only — C5 is single-core and headless).
    xTaskCreatePinnedToCore(display_task, "display", DISPLAY_TASK_STACK, NULL, 1, NULL, 1);
#endif

    Serial.println("[BOOT] All tasks started — entering main loop");
}

void loop() {
    serial_cmd_poll();
    delay(10);
}
