#pragma once

#if HUGINN_HAS_GPS

#include <stdint.h>

struct GpsPosition {
    bool   fix;
    double lat;
    double lon;
    float  speed_kph;
    uint32_t rmc_count;   // RMC sentences received since boot (0 = no data: check wiring)
};

void        gps_reader_init();
GpsPosition gps_get_position();

#endif // HUGINN_HAS_GPS
