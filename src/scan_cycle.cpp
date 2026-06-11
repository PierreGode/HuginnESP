#include "scan_cycle.h"
#include "ble_scanner.h"

static volatile bool s_running = true;

void scan_cycle_init() {
    s_running = true;
}

void scan_cycle_pause() {
    s_running = false;
}

void scan_cycle_resume() {
    s_running = true;
}

bool scan_cycle_is_running() {
    return s_running;
}

void scan_cycle_task(void* param) {
    (void)param;
    bool scanning = false;

    for (;;) {
        if (!s_running) {
            if (scanning) {
                ble_scanner_stop();
                scanning = false;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!scanning) {
            ble_scanner_start(BLE_MODE_SKIMMER);
            scanning = true;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
