// =====================================================================
//  gps_uart.cpp — minimal NMEA GPS reader (optional, build-flag gated).
//
//  Parses just $GPRMC + $GPGGA — enough to stamp scan detections with
//  lat/lon/alt for WiGLE export when a hardware GPS is wired up.
//  When HUGINN_GPS_UART=0 (the default) everything compiles to no-ops.
// =====================================================================

#include "gps_uart.h"

#if !HUGINN_GPS_UART

void gps_uart_init() {}
bool  gps_have_fix() { return false; }
float gps_lat()      { return 0.0f; }
float gps_lon()      { return 0.0f; }
int   gps_alt_m()    { return 0; }

#else

#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifndef HUGINN_GPS_UART_NUM
  #define HUGINN_GPS_UART_NUM 1
#endif
#ifndef HUGINN_GPS_BAUD
  #define HUGINN_GPS_BAUD 9600
#endif
#ifndef HUGINN_GPS_RX_PIN
  #define HUGINN_GPS_RX_PIN 17
#endif
#ifndef HUGINN_GPS_TX_PIN
  #define HUGINN_GPS_TX_PIN 16
#endif

static HardwareSerial s_gps(HUGINN_GPS_UART_NUM);
static volatile bool  s_fix = false;
static volatile float s_lat = 0.0f, s_lon = 0.0f;
static volatile int   s_alt = 0;

// NMEA fields look like "ddmm.mmmm,N" — decode to signed decimal degrees.
static bool parseLatLon(const char* field, char hem, float& out) {
    if (!field || !*field) return false;
    float raw = atof(field);
    int   deg = (int)(raw / 100.0f);
    float min = raw - (deg * 100.0f);
    out = deg + (min / 60.0f);
    if (hem == 'S' || hem == 'W') out = -out;
    return true;
}

// Split a NMEA sentence into up to 16 comma fields in-place.
static int splitFields(char* line, char* fields[], int max) {
    int n = 0;
    fields[n++] = line;
    for (char* p = line; *p && n < max; p++) {
        if (*p == ',') { *p = 0; fields[n++] = p + 1; }
        else if (*p == '*') { *p = 0; break; }
    }
    return n;
}

static void parseLine(char* line) {
    char* f[16];
    int n = splitFields(line, f, 16);
    if (n < 1) return;

    if (strstr(f[0], "RMC") && n >= 7) {
        // $..RMC,time,A/V,lat,N/S,lon,E/W,...
        bool valid = (f[2][0] == 'A');
        if (valid) {
            float lat, lon;
            if (parseLatLon(f[3], f[4][0], lat) && parseLatLon(f[5], f[6][0], lon)) {
                s_lat = lat; s_lon = lon; s_fix = true;
            }
        } else {
            s_fix = false;
        }
    } else if (strstr(f[0], "GGA") && n >= 10) {
        // $..GGA,time,lat,N/S,lon,E/W,fix,sats,hdop,alt,M,...
        s_alt = (int)atof(f[9]);
    }
}

static void gps_task(void*) {
    char buf[120]; size_t blen = 0;
    for (;;) {
        while (s_gps.available()) {
            char c = (char)s_gps.read();
            if (c == '\r') continue;
            if (c == '\n') {
                if (blen > 6 && buf[0] == '$') { buf[blen] = 0; parseLine(buf); }
                blen = 0;
            } else if (blen < sizeof(buf) - 1) {
                buf[blen++] = c;
            } else {
                blen = 0;  // overflow — drop line
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void gps_uart_init() {
    s_gps.begin(HUGINN_GPS_BAUD, SERIAL_8N1, HUGINN_GPS_RX_PIN, HUGINN_GPS_TX_PIN);
    xTaskCreate(gps_task, "huginn_gps", 4096, NULL, 1, NULL);
}

bool  gps_have_fix() { return s_fix; }
float gps_lat()      { return s_lat; }
float gps_lon()      { return s_lon; }
int   gps_alt_m()    { return s_alt; }

#endif // HUGINN_GPS_UART
