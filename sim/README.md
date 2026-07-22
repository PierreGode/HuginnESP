# scan_cycle_sim

Native simulation of the ESP32-C5 wardrive scan-cycle radio + heap lifecycle,
used to decide how to handle WiFi vs 802.15.4 (which share one 2.4 GHz radio).

Run:

```
g++ -std=c++17 -O2 scan_cycle_sim.cpp -o sim && ./sim
```

Conclusion: WiFi and 802.15.4 cannot be interleaved in the wardrive cycle on the
C5 — once 802.15.4 receives, WiFi cannot reclaim the shared radio, and forcing a
WiFi re-init exhausts the ~116 KB heap. Wardrive therefore runs WiFi + BLE only;
Zigbee/Thread is a separate on-demand mode (`zigbee` command).
