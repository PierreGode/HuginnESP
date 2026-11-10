// Host preview of the T-Dongle-C5 UI: renders src/tdongle_ui.cpp with fake
// scan data and writes raw RGB frames for sim/tdongle_ui_preview.py.
//
//   g++ -std=c++17 -O2 -I../src tdongle_ui_sim.cpp ../src/tdongle_ui.cpp -o tdsim
//   ./tdsim out_dir && python3 tdongle_ui_preview.py out_dir preview.png
#include "tdongle_ui.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static uint16_t fb[TD_UI_W * TD_UI_H];

struct FakeAp { const char* ssid; int rssi; int ch; TdUiSec sec; };
static const FakeAp APS[] = {
    {"Valhalla-5G", -41, 36, TD_SEC_WPA3}, {"Valhalla", -47, 6, TD_SEC_WPA},
    {"Mimir_Guest", -58, 11, TD_SEC_OPEN}, {"Yggdrasil", -63, 1, TD_SEC_WPA},
    {"", -66, 44, TD_SEC_WPA},             {"Bifrost-Mesh", -70, 149, TD_SEC_WPA},
    {"Sleipnir", -74, 1, TD_SEC_WPA},      {"FenrirNet", -77, 100, TD_SEC_WPA},
    {"Asgard IoT", -80, 6, TD_SEC_WPA},    {"Jormungandr-2.4", -83, 13, TD_SEC_WEP},
    {"Ratatoskr", -86, 157, TD_SEC_WPA},   {"Niflheim", -88, 9, TD_SEC_WPA},
    {"Draupnir", -72, 52, TD_SEC_WPA3},    {"Gungnir", -79, 3, TD_SEC_WPA},
};

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    td_ui_init();
    TdUiStatus st{};
    st.mode = "wardrive"; st.fw = "1.0"; st.freeHeapK = 96;
    int frame = 0;
    for (uint32_t now = 1000; now < 1000 + 62000; now += 33) {
        uint32_t t = now - 1000;
        // Fake sightings: APs trickle in after the splash, BLE noise throughout.
        if (t > 3300 && (t / 33) % 6 == 0) {
            int n = sizeof(APS) / sizeof(APS[0]);
            int limit = (int)((t - 3300) / 500) + 3; if (limit > n) limit = n;
            for (int i = 0; i < limit; i++)
                td_ui_note_wifi(now, 1000 + i * 7919, APS[i].ssid,
                                APS[i].rssi + (rand() % 5) - 2, APS[i].ch, APS[i].sec);
            for (int i = 0; i < 22; i++)
                td_ui_note_ble(now, 50000 + i * 104729, "aa:bb:cc:dd:ee:ff", -50 - (i * 37) % 45, 0);
        }
        if (t >= 44000 && t < 44040)
            td_ui_note_ble(now, 777, "80:e1:26:00:f1:1b", -52, TD_BLE_FLIPPER);
        if (t >= 44000) st.flippers = 1;
        st.nowMs = now; st.sessWifi = 14 + t / 900; st.sessBle = 22 + t / 400;
        td_ui_render(fb, st);
        char path[512];
        snprintf(path, sizeof(path), "%s/f%05d.rgb", dir.c_str(), frame++);
        FILE* f = fopen(path, "wb");
        if (!f) { perror(path); return 1; }
        for (int i = 0; i < TD_UI_W * TD_UI_H; i++) {
            uint16_t c = fb[i];
            unsigned char rgb[3] = { (unsigned char)(((c >> 11) & 31) * 255 / 31),
                                     (unsigned char)(((c >> 5) & 63) * 255 / 63),
                                     (unsigned char)((c & 31) * 255 / 31) };
            fwrite(rgb, 1, 3, f);
        }
        fclose(f);
    }
    printf("%d frames\n", frame);
    return 0;
}
