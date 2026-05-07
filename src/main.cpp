// =====================================================================
//  Ragnar ESP32-S3 Scanner Firmware
//  Board: Waveshare ESP32-S3-Touch-LCD-4B (N16R8, 4" 480×480 RGB touch)
//
//  Scans WiFi & BLE, detects Flipper Zero / AirTag / Skimmer /
//  Evil-Twin / BLE spam.  Outputs data over USB serial (115200) in
//  formats compatible with Ragnar's wardriving.py parser.
// =====================================================================

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

static int deviceCount = 0;

class MyCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        deviceCount++;
    }
};

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("[BOOT] === BLE Test (Arduino 3.x) ===");
    Serial.printf("[BOOT] Free heap: %u\n", ESP.getFreeHeap());
    Serial.printf("[BOOT] PSRAM size: %u\n", ESP.getPsramSize());
    Serial.flush();

    Serial.println("[BLE] Initializing...");
    Serial.flush();
    BLEDevice::init("HuginnESP");
    Serial.println("[BLE] Init done!");
    Serial.flush();

    BLEScan* pScan = BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new MyCallbacks(), false);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    Serial.println("[BLE] Starting 5s scan...");
    Serial.flush();
    deviceCount = 0;
    pScan->start(5, false);
    pScan->stop();
    Serial.printf("[BLE] Scan done! Found %d devices\n", deviceCount);
    Serial.printf("[BOOT] Free heap after BLE: %u\n", ESP.getFreeHeap());
    Serial.flush();
}

void loop() {
    Serial.println("[LOOP] alive");
    Serial.flush();
    delay(5000);
}
