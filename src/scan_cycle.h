#ifndef SCAN_CYCLE_H
#define SCAN_CYCLE_H

#include <Arduino.h>

void scan_cycle_init();
void scan_cycle_task(void* param);
void scan_cycle_pause();
void scan_cycle_resume();
bool scan_cycle_is_running();

#endif // SCAN_CYCLE_H
