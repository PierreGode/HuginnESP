#ifndef GPS_UART_H
#define GPS_UART_H

// =====================================================================
//  Optional UART GPS — NEO-6M / NEO-M9N style.
//
//  Compiled in only when -DHUGINN_GPS_UART=1 is set at build time.
//  Without that flag the public API resolves to no-ops, so the rest of
//  the firmware can call gps_*() unconditionally.
//
//  Wiring (S3 default — override via build flags):
//    GPS TX -> GPIO 17 (HUGINN_GPS_RX_PIN)
//    GPS RX -> GPIO 16 (HUGINN_GPS_TX_PIN)
//    GPS GND/3V3 from board.
//
//  Output: every detection picked up by the scan_event_bus gets stamped
//  with the most recent GPS fix when one is available. The browser-side
//  WiGLE export prefers the firmware fix and falls back to the phone's
//  navigator.geolocation when no firmware GPS is present.
// =====================================================================

#include <Arduino.h>

#ifndef HUGINN_GPS_UART
  #define HUGINN_GPS_UART 0
#endif

void gps_uart_init();   // no-op when HUGINN_GPS_UART=0

bool  gps_have_fix();
float gps_lat();
float gps_lon();
int   gps_alt_m();

#endif // GPS_UART_H
