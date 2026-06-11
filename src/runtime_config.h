#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

#include <Arduino.h>

// Runtime config currently holds the skimmer name fingerprints only.
// Values are RAM-only and reset on boot.

bool   isSkimmerName(const String& name);
String getSkimmerNamesCsv();

void runtime_config_init();

#endif // RUNTIME_CONFIG_H
