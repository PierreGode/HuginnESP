#include "gps_reader.h"

#if HUGINN_HAS_GPS

#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <string.h>
#include <stdlib.h>

static SemaphoreHandle_t s_mutex    = nullptr;
static bool              s_hasFix   = false;
static double            s_lat      = 0.0;
static double            s_lon      = 0.0;
static float             s_speedKph = 0.0f;
static volatile uint32_t s_rmcCount = 0;
// From GGA — only used for the 1 Hz position telemetry line.
static int               s_sats     = -1;
static float             s_hdop     = -1.0f;
static float             s_alt      = 0.0f;
static bool              s_hasAlt   = false;
static uint32_t          s_lastTelemMs = 0;

static HardwareSerial s_gpsSerial(GPS_UART_NUM);

// Convert NMEA DDMM.MMMM / DDDMM.MMMM to decimal degrees.
static double nmeaToDeg(const char* field, char dir) {
    double raw = atof(field);
    int    deg = (int)(raw / 100);
    double min = raw - deg * 100.0;
    double result = deg + min / 60.0;
    if (dir == 'S' || dir == 'W') result = -result;
    return result;
}

// Parse one RMC sentence (any talker ID). Updates shared state on valid fix.
static void parseRMC(char* sentence) {
    // Strip checksum suffix (*HH).
    char* star = strchr(sentence, '*');
    if (star) *star = '\0';

    // Tokenise into fields.
    char* f[12];
    int   n = 0;
    char* tok = strtok(sentence, ",");
    while (tok && n < 12) {
        f[n++] = tok;
        tok = strtok(nullptr, ",");
    }
    // Minimum: type(0) time(1) status(2) lat(3) NS(4) lon(5) EW(6)
    if (n < 7) return;

    // Status 'A' = active fix; 'V' = void.
    if (f[2][0] != 'A') {
        if (s_mutex && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            s_hasFix = false;
            xSemaphoreGive(s_mutex);
        }
        return;
    }

    if (strlen(f[3]) < 4 || strlen(f[5]) < 5 ||
        f[4][0] == '\0'  || f[6][0] == '\0') return;

    double lat   = nmeaToDeg(f[3], f[4][0]);
    double lon   = nmeaToDeg(f[5], f[6][0]);
    float  speed = (n > 7 && strlen(f[7]) > 0) ? atof(f[7]) * 1.852f : 0.0f;

    if (s_mutex && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        s_hasFix   = true;
        s_lat      = lat;
        s_lon      = lon;
        s_speedKph = speed;
        xSemaphoreGive(s_mutex);
    }
}

// Split an NMEA sentence in place on ',' keeping empty fields (strtok would
// collapse them and shift every later field). Returns the field count.
static int splitFields(char* s, char** f, int maxf) {
    int n = 0;
    f[n++] = s;
    for (char* p = s; *p && n < maxf; p++) {
        if (*p == ',') { *p = '\0'; f[n++] = p + 1; }
    }
    return n;
}

// Parse one GGA sentence (any talker) for satellites / HDOP / altitude.
static void parseGGA(char* sentence) {
    char* star = strchr(sentence, '*');
    if (star) *star = '\0';
    char* f[15];
    // type(0) time(1) lat(2) NS(3) lon(4) EW(5) quality(6) sats(7) hdop(8) alt(9)
    if (splitFields(sentence, f, 15) < 10) return;
    s_sats   = f[7][0] ? atoi(f[7]) : -1;
    s_hdop   = f[8][0] ? (float)atof(f[8]) : -1.0f;
    s_hasAlt = f[9][0] != '\0';
    if (s_hasAlt) s_alt = (float)atof(f[9]);
}

// One compact position line per second while a fix is held, e.g.
//   {"type":"GPS","lat":59.3293000,"lon":18.0686000,"speed_kmh":12.3,"sats":9,"hdop":0.9,"alt":31.2}
// Carries no "mac", so hosts treat it as position-only telemetry (Ragnar feeds
// it to its GPS manager as an external fix). Nothing is sent without a fix.
static void emitTelemetry() {
    uint32_t now = millis();
    if (now - s_lastTelemMs < GPS_TELEMETRY_MS) return;
    GpsPosition p = gps_get_position();
    if (!p.fix) return;
    s_lastTelemMs = now;
    char buf[192];
    int n = snprintf(buf, sizeof(buf),
                     "{\"type\":\"GPS\",\"lat\":%.7f,\"lon\":%.7f,\"speed_kmh\":%.1f",
                     p.lat, p.lon, p.speed_kph);
    if (s_sats >= 0)    n += snprintf(buf + n, sizeof(buf) - n, ",\"sats\":%d", s_sats);
    if (s_hdop >= 0.0f) n += snprintf(buf + n, sizeof(buf) - n, ",\"hdop\":%.1f", s_hdop);
    if (s_hasAlt)       n += snprintf(buf + n, sizeof(buf) - n, ",\"alt\":%.1f", s_alt);
    snprintf(buf + n, sizeof(buf) - n, "}");
    Serial.println(buf);
}

static void gps_task(void*) {
    char line[128];
    int  pos = 0;

    for (;;) {
        while (s_gpsSerial.available()) {
            char c = (char)s_gpsSerial.read();
            if (c == '\n' || c == '\r') {
                if (pos > 0) {
                    line[pos] = '\0';
                    // Any talker's RMC: $GPRMC (GPS), $GNRMC (multi-GNSS, e.g.
                    // ATGM336H default), $BDRMC / $GBRMC (BeiDou-only), $GLRMC ...
                    if (line[0] == '$' && pos >= 6 &&
                        strncmp(line + 3, "RMC", 3) == 0) {
                        s_rmcCount = s_rmcCount + 1;
                        parseRMC(line);
                        emitTelemetry();
                    } else if (line[0] == '$' && pos >= 6 &&
                               strncmp(line + 3, "GGA", 3) == 0) {
                        parseGGA(line);
                    }
                    pos = 0;
                }
            } else if (pos < (int)sizeof(line) - 1) {
                line[pos++] = c;
            } else {
                pos = 0; // line overflow — discard and resync
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void gps_reader_init() {
    if (s_mutex) return;
    s_mutex = xSemaphoreCreateMutex();
    s_gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.printf("[GPS] UART%d RX=%d TX=%d baud=%d\n",
                  GPS_UART_NUM, GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
    xTaskCreate(gps_task, "gps", GPS_TASK_STACK, nullptr, 1, nullptr);
}

GpsPosition gps_get_position() {
    GpsPosition p{};
    if (!s_mutex) return p;
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        p.fix       = s_hasFix;
        p.lat       = s_lat;
        p.lon       = s_lon;
        p.speed_kph = s_speedKph;
        xSemaphoreGive(s_mutex);
        p.rmc_count = s_rmcCount;
    }
    return p;
}

#endif // HUGINN_HAS_GPS
