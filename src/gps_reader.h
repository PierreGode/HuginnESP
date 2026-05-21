#pragma once

#if HUGINN_HAS_GPS

struct GpsPosition {
    bool   fix;
    double lat;
    double lon;
    float  speed_kph;
};

void        gps_reader_init();
GpsPosition gps_get_position();

#endif // HUGINN_HAS_GPS
