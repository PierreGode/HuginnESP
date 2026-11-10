// =====================================================================
//  HuginnESP — Multi-board ESP32 scanner firmware
//
//  Supported boards (selected via PlatformIO env / build flags):
//    HUGINN_BOARD_S3 + HUGINN_HAS_DISPLAY=1
//        Waveshare ESP32-S3-Touch-LCD-4B (N16R8, 4" 480x480 RGB touch)
//    HUGINN_BOARD_C5 + HUGINN_HAS_DISPLAY=0
//        Waveshare ESP32-C5-WIFI6-KIT (16MB/8MB PSRAM, dual-band Wi-Fi 6, no display)
//    HUGINN_BOARD_C5 + HUGINN_BOARD_XIAO_C5
//        Seeed XIAO ESP32-C5 (8MB flash, headless; built via scripts/build-xiao.sh)
//    HUGINN_BOARD_C5 + HUGINN_BOARD_C5_WROOM
//        Any ESP32-C5-WROOM-1 / WROOM-1U board (>=8MB flash, headless)
//    HUGINN_BOARD_C5 + HUGINN_BOARD_TDONGLE_C5 + HUGINN_HAS_DISPLAY=1
//        LilyGO T-Dongle-C5 (16MB flash, 0.96" 160x80 ST7735, animated UI)
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
#include "zigbee_scanner.h"
#include "serial_cmd.h"
#include "scan_cycle.h"
#include "runtime_config.h"
#include "skimmer_led.h"
#include "button.h"
#if HUGINN_HAS_GPS
#include "gps_reader.h"
#endif

void setup() {
    Serial.begin(SERIAL_BAUD);
#if HUGINN_HAS_DISPLAY && HUGINN_BOARD_TDONGLE_C5
    // Wi-Fi init stalls the whole (single-core) chip for ~0.6 s while the PHY
    // calibrates, which would freeze the boot animation mid-flight. So on this
    // board Wi-Fi comes up first, on a still-dark screen, and the display (with
    // its own render task) starts right after — the splash then plays smoothly
    // through the serial settle delay and the BLE / Zigbee init below.
    runtime_config_init();
    wifi_scanner_init();
    display_init();
#endif
    delay(2000);

#if HUGINN_BOARD_S3
    const char* HUGINN_BOARD_NAME = "esp32-s3";
#elif HUGINN_BOARD_C5
    const char* HUGINN_BOARD_NAME = "esp32-c5";
#elif HUGINN_BOARD_GENERIC
    const char* HUGINN_BOARD_NAME = "esp32";
#else
    const char* HUGINN_BOARD_NAME = "unknown";
#endif
    // Build the capability list dynamically so each optional feature adds its
    // own token without a combinatorial nest of #if branches.
    char HUGINN_CAPS[96];
    strlcpy(HUGINN_CAPS, "\"wifi\",\"ble\"", sizeof(HUGINN_CAPS));
#if HUGINN_HAS_ZIGBEE
    strlcat(HUGINN_CAPS, ",\"zigbee\"", sizeof(HUGINN_CAPS));
#endif
#if HUGINN_HAS_DISPLAY
    strlcat(HUGINN_CAPS, ",\"display\"", sizeof(HUGINN_CAPS));
#endif
#if HUGINN_HAS_GPS
    strlcat(HUGINN_CAPS, ",\"gps\"", sizeof(HUGINN_CAPS));
#endif
    // Device announce — emitted first so the host can distinguish HuginnESP
    // from other ESP32 firmware sharing the USB bus.
    Serial.printf("{\"device\":\"HuginnESP\",\"fw\":\"%s\",\"board\":\"%s\",\"caps\":[%s]}\n",
                  HUGINN_FW_VERSION, HUGINN_BOARD_NAME, HUGINN_CAPS);

    Serial.println("[BOOT] HuginnESP starting...");
#if HUGINN_BOARD_S3
    Serial.println("[BOOT] Board: ESP32-S3-Touch-LCD-4B (display)");
#elif HUGINN_BOARD_C5 && HUGINN_BOARD_TDONGLE_C5
    Serial.println("[BOOT] Board: LilyGO T-Dongle-C5 (160x80 display, dual-band Wi-Fi 6)");
#elif HUGINN_BOARD_C5 && HUGINN_BOARD_XIAO_C5
    Serial.println("[BOOT] Board: Seeed XIAO ESP32-C5 (headless, dual-band Wi-Fi 6)");
#elif HUGINN_BOARD_C5 && HUGINN_BOARD_C5_WROOM
    Serial.println("[BOOT] Board: ESP32-C5-WROOM-1/1U (headless, dual-band Wi-Fi 6)");
#elif HUGINN_BOARD_C5
    Serial.println("[BOOT] Board: ESP32-C5-WIFI6-KIT (headless, dual-band Wi-Fi 6)");
#elif HUGINN_BOARD_GENERIC
    Serial.println("[BOOT] Board: Generic ESP32 (headless, 2.4 GHz)");
#else
    Serial.println("[BOOT] Board: unknown");
#endif
    Serial.printf("[BOOT] Free heap: %u\n", ESP.getFreeHeap());
    Serial.printf("[BOOT] PSRAM: %u\n", ESP.getPsramSize());
    Serial.flush();

#if !(HUGINN_HAS_DISPLAY && HUGINN_BOARD_TDONGLE_C5)   // T-Dongle did these above
    runtime_config_init();

    // WiFi MUST init before display — WiFi ISR conflicts with RGB DMA cache.
    Serial.println("[BOOT] Init WiFi...");
    wifi_scanner_init();
    Serial.println("[BOOT] WiFi OK");
#endif

#if HUGINN_HAS_GPS
    Serial.println("[BOOT] Init GPS...");
    gps_reader_init();
    Serial.println("[BOOT] GPS UART started");
#endif

#if HUGINN_HAS_DISPLAY && !HUGINN_BOARD_TDONGLE_C5
    Serial.println("[BOOT] Init display...");
    display_init();
    Serial.println("[BOOT] Display OK");
#endif

    Serial.println("[BOOT] Init BLE...");
    ble_scanner_init();
    Serial.println("[BOOT] BLE OK");

#if HUGINN_HAS_ZIGBEE
    Serial.println("[BOOT] Init Zigbee (802.15.4)...");
    zigbee_scanner_init();
    Serial.println("[BOOT] Zigbee init done");
#endif

    // Skimmer proximity LED (no-op unless -DHUGINN_HAS_SKIMMER_LED=1).
    skimmer_led_init();

    // Mode button (no-op unless -DHUGINN_HAS_MODE_BUTTON=1).
    button_init();

    serial_cmd_init();
    scan_cycle_init();

    Serial.printf("[BOOT] Free heap after init: %u\n", ESP.getFreeHeap());
    Serial.flush();

    // Pin scan cycle to core 0 (works on dual-core S3 and single-core C5).
    xTaskCreatePinnedToCore(scan_cycle_task, "scan_cycle", CYCLE_TASK_STACK, NULL, 1, NULL, 0);
#if HUGINN_HAS_DISPLAY && !HUGINN_BOARD_TDONGLE_C5
    // Display rendering on core 1 (S3 only — the single-core T-Dongle-C5
    // starts its own render task from display_init()).
    xTaskCreatePinnedToCore(display_task, "display", DISPLAY_TASK_STACK, NULL, 1, NULL, 1);
#endif

    Serial.println("[BOOT] All tasks started — entering main loop");
}

void loop() {
    serial_cmd_poll();
    delay(10);
}
