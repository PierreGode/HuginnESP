#ifndef ZIGBEE_SCANNER_H
#define ZIGBEE_SCANNER_H

#include <Arduino.h>
#include "config.h"

// Optional Zigbee / IEEE 802.15.4 scanner.
//
// Enabled with -DHUGINN_HAS_ZIGBEE=1 on boards whose SoC has an 802.15.4 radio
// (ESP32-C5 / C6 / H2). It puts the 802.15.4 MAC into promiscuous receive and
// listens on the 2.4 GHz Zigbee channels (11–26). Every received frame that
// carries a source address is parsed and emitted over serial as a single JSON
// line so the host (Ragnar) can count and map it:
//
//   {"type":"ZIGBEE","panid":"0x1A2B","addr":"AABBCCDDEEFF0011",
//    "channel":15,"rssi":-70,"lqi":180,"ftype":"beacon"}
//
// `addr` is the 64-bit extended source address (EUI-64, canonical MSB-first)
// when the frame carries one; frames with only a 16-bit short source address
// emit `"short":"0x1234"` instead. Channel hopping is driven by the caller via
// zigbee_scanner_set_channel().
//
// The radio is shared with WiFi and BLE on these single-radio SoCs, so the
// scan cycle parks WiFi and BLE for the duration of a Zigbee phase.

#if HUGINN_HAS_ZIGBEE

// Enable the 802.15.4 radio in promiscuous mode. Safe to call once at boot.
void zigbee_scanner_init();

// Begin a sniff phase: start receiving on the current channel and reset the
// per-phase device counter.
void zigbee_scanner_start();

// Stop receiving (frees the shared radio for WiFi/BLE).
void zigbee_scanner_stop();

// Retune to an 802.15.4 channel (11–26). Out-of-range values are ignored.
void zigbee_scanner_set_channel(uint8_t channel);

// Unique devices seen during the current phase / since boot.
int zigbee_scanner_count();
int zigbee_scanner_session_count();

#else

// Compiled-out no-ops so callers don't need their own #if guards.
static inline void zigbee_scanner_init() {}
static inline void zigbee_scanner_start() {}
static inline void zigbee_scanner_stop() {}
static inline void zigbee_scanner_set_channel(uint8_t) {}
static inline int  zigbee_scanner_count() { return 0; }
static inline int  zigbee_scanner_session_count() { return 0; }

#endif // HUGINN_HAS_ZIGBEE

#endif // ZIGBEE_SCANNER_H
