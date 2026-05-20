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

// Parse one $GPRMC / $GNRMC sentence. Updates shared state on valid fix.
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

static void gps_task(void*) {
    char line[128];
    int  pos = 0;

    for (;;) {
        while (s_gpsSerial.available()) {
            char c = (char)s_gpsSerial.read();
            if (c == '\n' || c == '\r') {
                if (pos > 0) {
                    line[pos] = '\0';
                    if (strncmp(line, "$GPRMC", 6) == 0 ||
                        strncmp(line, "$GNRMC", 6) == 0) {
                        parseRMC(line);
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
    }
    return p;
}

#endif // HUGINN_HAS_GPS
