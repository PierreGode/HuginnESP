#ifndef SCAN_EVENT_BUS_H
#define SCAN_EVENT_BUS_H

#include <Arduino.h>
#include <stdint.h>

// =====================================================================
//  Scan event bus
//
//  Decouples scanner output from sinks. Each detection is pushed once;
//  the serial path (Ragnar) keeps reading the existing printf-to-Serial
//  output, and the web/WebSocket path drains a ring buffer that mirrors
//  the same events. New sinks (e.g. SD card logger, MQTT bridge) can be
//  added without touching the scanner code paths.
//
//  All access is thread-safe (FreeRTOS mutex). Snapshot copies are made
//  for the HTTP layer so a slow consumer can't stall the BLE callback.
// =====================================================================

enum ScanEventType : uint8_t {
    SCAN_EVT_WIFI = 1,
    SCAN_EVT_BLE  = 2,
};

struct ScanEvent {
    uint8_t  type;           // ScanEventType
    uint32_t ts_ms;          // millis() at insertion
    char     mac[18];        // "AA:BB:CC:DD:EE:FF"
    char     ssid_or_name[33];
    int8_t   rssi;
    uint8_t  channel;        // WiFi only; 0 for BLE
    uint8_t  auth;           // wifi_auth_mode_t cast; 0 for BLE
    uint8_t  flags;          // bit0=flipper bit1=airtag bit2=skimmer (BLE)
    // Optional GPS stamp at detection time (firmware-side GPS, when present).
    // NaN-ish sentinel = no fix. The browser-side WiGLE export will fall
    // back to navigator.geolocation when these are absent.
    float    lat;
    float    lon;
    int16_t  alt_m;
    uint8_t  gps_fix;        // 0 = no fix, 1 = 2D/3D
    uint8_t  _pad;
};
static_assert(sizeof(ScanEvent) <= 80, "ScanEvent should stay compact");

void scan_event_bus_init();

void scan_event_bus_push_wifi(const char* mac, const char* ssid,
                              int8_t rssi, uint8_t channel, uint8_t auth);

// BLE flags bitfield: bit0=flipper, bit1=airtag, bit2=skimmer
void scan_event_bus_push_ble(const char* mac, const char* name,
                             int8_t rssi, uint8_t flags);

// Copy up to `max` most-recent events into out[]. Returns number copied,
// newest first. Caller-owned buffer; safe to call from any task.
size_t scan_event_bus_snapshot(ScanEvent* out, size_t max);

// Monotonic counter — every push increments it. Lets the web client poll
// "has anything changed since cursor N?" without copying the whole buffer.
uint32_t scan_event_bus_seq();

#endif // SCAN_EVENT_BUS_H
