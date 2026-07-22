// HuginnESP C5 scan-cycle simulation.
//
// Purpose: stop guessing. Model the two real, coupled constraints on the
// ESP32-C5 — (1) one shared 2.4 GHz radio between WiFi / BLE / 802.15.4, and
// (2) a tight dynamic heap — and run the ACTUAL wardrive cycle sequence under
// each fix strategy to see which one keeps WiFi working every cycle without
// starving the heap (which is what killed Zigbee).
//
// Assumptions are documented and calibrated to observed device facts:
//   * Boot log: "[BOOT] Free heap after init: 116516"  (WiFi+BLE+802154 all up)
//   * Hardware behaviour observed across reflashes:
//       - cycle-1 WiFi phase (before any 802.15.4 receive) captures 2.4+5 GHz
//       - every WiFi phase AFTER the first Zigbee sweep returns 0 networks
//       - disabling 802.15.4 after the sweep did NOT restore WiFi
//       - re-initing WiFi each cycle broke WiFi AND made Zigbee disappear
//
// Build:  g++ -std=c++17 -O2 sim.cpp -o sim && ./sim

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

// ---- Heap model ------------------------------------------------------------
// ESP-IDF-typical resident footprints (bytes). These are order-of-magnitude
// figures used only to reproduce the *relationship* observed on the device;
// they are calibrated so the post-init free heap matches the 116516 the boot
// log printed.
static const long WIFI_CORE   = 58000;  // esp_wifi rx/tx buffers + state
static const long NIMBLE_CORE = 40000;  // NimBLE host+controller (started once)
static const long IEEE802154  = 8000;   // 802.15.4 driver when enabled
// Total dynamic heap chosen so: TOTAL - (WIFI+NIMBLE+802154) = 116516 observed.
static const long HEAP_TOTAL  = 116516 + WIFI_CORE + NIMBLE_CORE + IEEE802154; // 222516

struct Heap {
    long total = HEAP_TOTAL;
    long used  = 0;
    long min_free = HEAP_TOTAL;
    bool oom = false;
    long alloc(long n) {           // returns 0 on success, -1 on OOM
        if (used + n > total) { oom = true; return -1; }
        used += n;
        min_free = std::min(min_free, total - used);
        return 0;
    }
    void freeb(long n) { used -= n; if (used < 0) used = 0; }
    long freeNow() const { return total - used; }
};

// ---- Radio model -----------------------------------------------------------
// The 2.4 GHz radio is shared. The observed hardware fact we encode: once the
// 802.15.4 driver has been put into RECEIVE, the WiFi 2.4 GHz RX path is
// "poisoned" — the scan still completes but receives nothing — until the WiFi
// MAC/PHY is FULLY re-initialised (a mode cycle). Merely disabling 802.15.4
// does not clear it (matches: user disabled it, no change). 5 GHz WiFi uses the
// same single radio on the C5 too, so it is poisoned as well (matches: 5 GHz
// also stopped after the first Zigbee sweep).
struct Radio {
    bool wifi_inited = true;      // WiFi driver allocated & inited
    bool ieee_enabled = false;    // 802.15.4 enabled
    bool poisoned = false;        // set once 802.15.4 has received
};

// ---- Strategy knobs --------------------------------------------------------
struct Strategy {
    std::string name;
    bool run_zigbee_phase;   // is the 802.15.4 sweep part of the cycle?
    bool disable_ieee_after; // call esp_ieee802154_disable() after each sweep
    bool reinit_wifi;        // WiFi mode cycle (OFF->STA) each WiFi phase
    long reinit_leak;        // bytes leaked per WiFi re-init (IDF deinit/init hazard)
};

struct Result {
    int cycles_run = 0;
    int wifi_ok_cycles = 0;   // cycles where the WiFi phase actually received
    int zigbee_ok_cycles = 0; // cycles where the 802.15.4 enable succeeded
    long final_free = 0;
    long min_free = 0;
    bool oom = false;
    int oom_cycle = -1;
};

// One WiFi phase. Returns true if it actually receives networks.
static bool wifi_phase(Heap& h, Radio& r, const Strategy& s) {
    if (s.reinit_wifi) {
        // WiFi.mode(WIFI_OFF): free WiFi core, but IDF deinit/init can leak.
        h.freeb(WIFI_CORE);
        // WiFi.mode(WIFI_STA): re-alloc core + leak.
        if (h.alloc(WIFI_CORE + s.reinit_leak) != 0) {
            r.wifi_inited = false;         // re-init failed → WiFi dead
            return false;
        }
        r.poisoned = false;                // full re-init clears the poison
    }
    if (!r.wifi_inited) return false;
    // WiFi 2.4 + 5 GHz both ride the one radio; poisoned => receive nothing.
    return !r.poisoned;
}

// One Zigbee (802.15.4) phase. Returns true if it ran (enable succeeded).
static bool zigbee_phase(Heap& h, Radio& r, const Strategy& s) {
    if (!s.run_zigbee_phase) return false;
    if (!r.ieee_enabled) {
        if (h.alloc(IEEE802154) != 0) return false;  // OOM → Zigbee gone
        r.ieee_enabled = true;
    }
    // Put radio into receive → poisons WiFi RX until a full WiFi re-init.
    r.poisoned = true;
    if (s.disable_ieee_after) {
        h.freeb(IEEE802154);
        r.ieee_enabled = false;
        // NOTE: disabling does NOT clear the poison (observed on hardware).
    }
    return true;
}

static Result run(const Strategy& s, int cycles) {
    Heap h; Radio r;
    // Boot init: WiFi + NimBLE + 802.15.4 all brought up once (matches boot log).
    h.alloc(WIFI_CORE);
    h.alloc(NIMBLE_CORE);
    h.alloc(IEEE802154); r.ieee_enabled = true;   // main.cpp inits zigbee at boot
    // If a strategy disables 802.15.4 after sweeps, boot leaves it enabled until
    // the first sweep's stop(); model that by keeping it enabled here.

    Result res; res.min_free = h.freeNow();
    for (int c = 1; c <= cycles; ++c) {
        // Cycle order (real firmware): WiFi -> BLE -> Zigbee.
        bool wok = wifi_phase(h, r, s);
        // BLE phase: NimBLE already resident, no per-cycle alloc; coexists w/ WiFi.
        bool zok = zigbee_phase(h, r, s);

        res.cycles_run = c;
        if (wok) res.wifi_ok_cycles++;
        if (zok) res.zigbee_ok_cycles++;
        res.min_free = std::min(res.min_free, h.freeNow());
        if (h.oom && res.oom_cycle < 0) { res.oom = true; res.oom_cycle = c; }
    }
    res.final_free = h.freeNow();
    return res;
}

static void report(const Strategy& s, const Result& r) {
    printf("%-42s | wifi_ok %3d/%d | zigbee_ok %3d/%d | free %6ld B | min_free %6ld B | %s\n",
           s.name.c_str(),
           r.wifi_ok_cycles, r.cycles_run,
           r.zigbee_ok_cycles, r.cycles_run,
           r.final_free, r.min_free,
           r.oom ? ("OOM@cycle " + std::to_string(r.oom_cycle)).c_str() : "no OOM");
}

int main() {
    const int CYCLES = 400;  // ~1 hour at ~9s/cycle
    printf("HuginnESP C5 scan-cycle sim  (heap total=%ld B, free-after-init≈%ld B)\n",
           HEAP_TOTAL, HEAP_TOTAL - (WIFI_CORE + NIMBLE_CORE + IEEE802154));
    printf("%d cycles per run\n\n", CYCLES);

    std::vector<Strategy> strategies = {
        // name, run_zigbee, disable_ieee_after, reinit_wifi, reinit_leak
        {"S0 original (802154 enabled, never disabled)", true,  false, false, 0},
        {"S1 disable 802154 after sweep (PR#27)",         true,  true,  false, 0},
        {"S2 wifi reinit each cycle, leak=1500 (PR#28)",  true,  true,  true,  1500},
        {"S2b wifi reinit each cycle, leak=0 (ideal)",    true,  true,  true,  0},
        {"S3 remove zigbee phase (wifi+ble only)",        false, false, false, 0},
    };
    for (auto& s : strategies) report(s, run(s, CYCLES));

    printf("\nLeak sensitivity for the reinit approach (how many cycles until OOM kills Zigbee):\n");
    for (long leak : {0L, 200L, 500L, 1000L, 1500L, 3000L}) {
        Strategy s{"  reinit leak=" + std::to_string(leak), true, true, true, leak};
        Result r = run(s, 2000);
        printf("  leak=%5ld B/cycle -> %s\n", leak,
               r.oom ? ("OOM at cycle " + std::to_string(r.oom_cycle) +
                        " (~" + std::to_string(r.oom_cycle*9/60) + " min); wifi_ok before that="
                        + std::to_string(r.wifi_ok_cycles)).c_str()
                     : "no OOM in 2000 cycles");
    }
    return 0;
}
