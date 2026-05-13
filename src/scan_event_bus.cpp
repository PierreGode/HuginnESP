#include "scan_event_bus.h"
#include "gps_uart.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Capacity tuned so a full snapshot fits comfortably in one HTTP response
// (~80 bytes/event × 512 = 40 KB) and so a 4 s wardrive burst (~30 WiFi +
// ~50 BLE events) leaves plenty of headroom. Lives in internal RAM — the
// snapshot path is hit on every WebSocket frame.
#define BUS_CAPACITY 512

static ScanEvent          s_ring[BUS_CAPACITY];
static volatile uint32_t  s_head = 0;     // index of next write slot
static volatile uint32_t  s_seq  = 0;     // monotonic push counter
static SemaphoreHandle_t  s_mux  = nullptr;

void scan_event_bus_init() {
    if (s_mux) return;
    s_mux = xSemaphoreCreateMutex();
    memset(s_ring, 0, sizeof(s_ring));
}

static void copyStr(char* dst, size_t cap, const char* src) {
    if (!src) { dst[0] = 0; return; }
    size_t n = strnlen(src, cap - 1);
    memcpy(dst, src, n);
    dst[n] = 0;
}

static void push(const ScanEvent& evt) {
    if (!s_mux) return;
    if (xSemaphoreTake(s_mux, pdMS_TO_TICKS(10)) != pdTRUE) return;
    s_ring[s_head % BUS_CAPACITY] = evt;
    s_head++;
    s_seq++;
    xSemaphoreGive(s_mux);
}

static void stampGps(ScanEvent& e) {
    if (gps_have_fix()) {
        e.lat     = gps_lat();
        e.lon     = gps_lon();
        e.alt_m   = (int16_t)gps_alt_m();
        e.gps_fix = 1;
    }
}

void scan_event_bus_push_wifi(const char* mac, const char* ssid,
                              int8_t rssi, uint8_t channel, uint8_t auth) {
    ScanEvent e = {};
    e.type    = SCAN_EVT_WIFI;
    e.ts_ms   = millis();
    e.rssi    = rssi;
    e.channel = channel;
    e.auth    = auth;
    e.gps_fix = 0;
    copyStr(e.mac,          sizeof(e.mac),          mac);
    copyStr(e.ssid_or_name, sizeof(e.ssid_or_name), ssid);
    stampGps(e);
    push(e);
}

void scan_event_bus_push_ble(const char* mac, const char* name,
                             int8_t rssi, uint8_t flags) {
    ScanEvent e = {};
    e.type    = SCAN_EVT_BLE;
    e.ts_ms   = millis();
    e.rssi    = rssi;
    e.channel = 0;
    e.auth    = 0;
    e.flags   = flags;
    e.gps_fix = 0;
    copyStr(e.mac,          sizeof(e.mac),          mac);
    copyStr(e.ssid_or_name, sizeof(e.ssid_or_name), name);
    stampGps(e);
    push(e);
}

size_t scan_event_bus_snapshot(ScanEvent* out, size_t max) {
    if (!s_mux || !out || max == 0) return 0;
    if (xSemaphoreTake(s_mux, pdMS_TO_TICKS(10)) != pdTRUE) return 0;

    uint32_t total = (s_head < BUS_CAPACITY) ? s_head : BUS_CAPACITY;
    size_t n = (total < max) ? total : max;

    // Walk newest -> oldest. Newest is at (s_head - 1) mod CAPACITY.
    for (size_t i = 0; i < n; i++) {
        uint32_t idx = (s_head - 1 - i) % BUS_CAPACITY;
        out[i] = s_ring[idx];
    }
    xSemaphoreGive(s_mux);
    return n;
}

uint32_t scan_event_bus_seq() {
    return s_seq;
}
