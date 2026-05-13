#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

#include <Arduino.h>

// Runtime-tunable knobs. Defaults come from the matching #defines in config.h.
// Change at runtime via the serial command line:
//   set <key> <value>
//   get <key>
//   get all
// Reads are lock-free for the integer knobs (32-bit aligned writes are atomic
// on ESP32). The skimmer name list is guarded by an internal mutex because
// it's a vector of Strings touched by the BLE callback and the serial parser.
//
// State lives in RAM only — no persistence. Push the config from the host
// at startup (or whenever it changes).

extern volatile uint32_t g_wifiScanDurationMs;
extern volatile uint32_t g_bleSpamThreshold;
extern volatile uint32_t g_wardriveWifiMs;
extern volatile uint32_t g_wardriveBleMs;

bool   isSkimmerName(const String& name);
String getSkimmerNamesCsv();

void runtime_config_init();

// Try to handle `set ...` / `get ...`. Returns true if the line matched
// either form (success or validation error — both are "handled"); returns
// false to let the caller continue dispatching.
bool runtime_config_handle(const String& line);

#endif // RUNTIME_CONFIG_H
