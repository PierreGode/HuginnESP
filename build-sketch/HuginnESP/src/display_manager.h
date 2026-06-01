#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>

void display_init();
void display_task(void* param);

// Called from other modules to push alert events
void display_add_alert(const char* type, const char* mac, int rssi);

#endif // DISPLAY_MANAGER_H
