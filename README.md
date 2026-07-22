# HuginnESP

WiFi & BLE wardriving firmware for ESP32. The device performs the radio scanning and pushes a live stream of detections out over USB serial — any host can consume the stream. The reference consumer is [Ragnar](https://github.com/PierreGode/Ragnar)'s wardriving engine, but the protocol is plain newline-delimited JSON so anything that can read a serial port will work.

---

## Supported devices

| Board | MCU | Radio | Display |
|---|---|---|---|
| **Waveshare ESP32-S3-Touch-LCD-4B** | ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB PSRAM) | WiFi 2.4 GHz + BLE 5 | 4" 480×480 RGB touch (GT911) |
| **Waveshare ESP32-C5-WIFI6-KIT** | ESP32-C5-WROOM-1 N16R4 (16 MB flash, 4 MB PSRAM, RISC-V) | Dual-band WiFi 6 (2.4 / 5 GHz) + BLE 5 + 802.15.4 (Zigbee) | none (headless) |
| **Seeed XIAO ESP32-C5** | ESP32-C5 (8 MB flash, 8 MB PSRAM, RISC-V) | Dual-band WiFi 6 (2.4 / 5 GHz) + BLE 5 + 802.15.4 (Zigbee) | none (headless) |

All boards run the same firmware behavior; the C5 builds skip display code (`HUGINN_HAS_DISPLAY=0`). The two C5 boards share the same ESP32-C5 chip but differ in flash size and toolchain — see [Flashing the firmware](#flashing-the-firmware).

## Features

| Feature | Description |
|---|---|
| **WiFi Scan** | Scan WiFi networks (SSID, BSSID, RSSI, channel, security) — 2.4 GHz on S3, dual-band on C5 |
| **BLE Scan** | Scan BLE devices (MAC, name, RSSI) — Flipper / AirTag / skimmer classification is emitted as separate alert lines |
| **802.15.4 Scan (Zigbee / Thread)** | **ESP32-C5 only.** Promiscuous IEEE 802.15.4 sniff across channels 11–26; every frame with a source address is emitted (PAN ID, EUI-64 / short address, channel, RSSI, LQI) with a `proto` tag classifying it as `zigbee` / `thread` / `802.15.4`. Matter-over-Thread reports as `thread`. Runs automatically as a phase of wardrive, or on demand via the `zigbee` command. Not available on the S3 / classic ESP32 (no 802.15.4 radio) |
| **Flipper Zero Detection** | Identify Flipper Zero devices via BLE advertisement data |
| **AirTag / Find My Detection** | Flag Apple Find My trackers via BLE manufacturer data. Matches only the full offline-finding "separated" beacon (Apple company `0x004C`, type `0x12`, length `0x19`) — the high-confidence standalone-tracker signal — so it does **not** fire on the ambient Find My chatter every nearby iPhone/Mac/AirPods relays. Each unique tracker MAC alerts once (AirTag MACs rotate ~15 min, so a tag reappears under a fresh anonymous identity) |
| **BLE Spam Detection** | Detect BLE advertising spam attacks |
| **Skimmer Detection** | Identify potential skimmer devices (HC-05/HC-06 BLE modules) |
| **Proximity Alert LED** | Optional — onboard RGB LED blinks faster the closer a flagged device gets (RSSI-driven). Colors identify the alert: skimmer = red⇄white, Flipper Zero = blue⇄white. Enabled on C5 builds |
| **Mode Button** | Optional — long-press the onboard BOOT button to toggle wardrive ⇄ skimmer-only scanning. LED confirms: 3 purple blinks = skimmer, 3 green = wardrive. C5 builds boot into wardrive |
| **Touch Display** | Live status, touch buttons, alert panel with color coding (S3 only) |
| **Session Tally** | Display-side running totals (unique WiFi BSSIDs, BLE / Flipper / AirTag / skimmer MACs) since power-on; resets on reboot, S3 only |
| **Auto Scan Cycle** | Automatic rotation through all scan modes |
| **GPS tagging** | Optional — when a NMEA GPS module is wired and has a fix, `lat`/`lon` are appended to every `WIFI` JSON line |

---

## GPS wiring (optional)

Any NMEA module that outputs `$GPRMC` sentences at 9600 baud works (GT-U7, NEO-6M, L76, etc.).

| GPS pin | ESP32 pin | Notes |
|---|---|---|
| VCC | 3.3 V | Most breakouts are 3.3 V — check your module |
| GND | GND | |
| TX (GPS out) | GPIO 17 by default (Waveshare C5/S3/generic) | This is the data line into the ESP32 |
| RX (GPS in) | GPIO 18 by default (Waveshare C5/S3/generic) | Leave unconnected if module is receive-only |

For **Seeed XIAO ESP32-C5** builds produced by `scripts/build-xiao.sh`, Soldred GPS defaults are:
- `GPS_RX_PIN=12`
- `GPS_TX_PIN=1`
- `GPS_UART_NUM=1`

To use different pins, override in `platformio.ini`:

```ini
build_flags =
    ...
    -DHUGINN_HAS_GPS=1
    -DGPS_RX_PIN=16
    -DGPS_TX_PIN=15
    -DGPS_UART_NUM=1
```

Build with one of the GPS-enabled environments:

```
pio run -e esp32s3box-gps
pio run -e esp32c5-gps
pio run -e esp32-gps
```

---

## Proximity alert LED (C5)

Headless C5 builds (`esp32c5`, `esp32c5-gps`) drive the board's onboard
addressable RGB LED (the WS2812B on `RGB_BUILTIN`) as a "hotter/colder" locator.
Whenever a flagged device is seen during a BLE scan the LED blinks, and the
blink rate tracks signal strength — the closer you get (stronger RSSI), the
faster it blinks. The blink colors tell you *what* it found:

| Alert | Blink colors |
|---|---|
| Potential skimmer (suspicious BLE module — see [Skimmer Detection](#features)) | **red ⇄ white** |
| Flipper Zero | **blue ⇄ white** |

It's a single LED, so if both are nearby at once the most recently seen device
wins. The LED turns itself off a few seconds after the device drops out of
range.

This is enabled by default on the C5 environments via `-DHUGINN_HAS_SKIMMER_LED=1`.
The pin defaults to the board's `RGB_BUILTIN`. Tunables (override in
`platformio.ini` `build_flags`):

| Flag | Default | Meaning |
|---|---|---|
| `SKIMMER_LED_PIN` | `RGB_BUILTIN` | GPIO driving the addressable LED |
| `SKIMMER_LED_BRIGHTNESS` | `40` | Per-channel brightness (0–255) of the blink colors |
| `SKIMMER_LED_RSSI_NEAR` | `-45` | RSSI at/above which it blinks fastest |
| `SKIMMER_LED_RSSI_FAR` | `-95` | RSSI at/below which it blinks slowest |
| `SKIMMER_LED_FAST_MS` / `SKIMMER_LED_SLOW_MS` | `70` / `1000` | Blink half-period at closest / farthest range |
| `SKIMMER_LED_HOLD_MS` | `10000` | How long to keep blinking after the last sighting (bridges the WiFi-only gaps between BLE scans, e.g. in wardrive) |

> RSSI is a coarse proximity proxy — readings jump around with orientation and
> obstacles, so treat the blink rate as "warmer/colder," not a distance meter.

To enable it on another board with an addressable LED, add
`-DHUGINN_HAS_SKIMMER_LED=1` (and `-DSKIMMER_LED_PIN=<gpio>` if needed) to that
environment's `build_flags`.

---

## Mode button (C5)

On C5 builds the onboard **BOOT** button toggles the scan mode with a
**long-press** (~1 s) — no host or serial command needed:

- The device **boots into wardrive** mode.
- **Long-press** → switches to **skimmer-only** scanning; the LED confirms with
  **3 purple blinks**.
- **Long-press again** → switches back to **wardrive**; the LED confirms with
  **3 green blinks**.

Skimmer-only mode runs the BLE skimmer scan continuously (WiFi off) so the
proximity LED stays responsive; wardrive is the normal WiFi + BLE wardriving
cycle. The serial commands (`wardrive`, `capture -skimmer`, `stop`, …) still
work and stay in sync with the button.

Enabled by default on the C5 environments via `-DHUGINN_HAS_MODE_BUTTON=1`.
Tunables (override in `platformio.ini` `build_flags`):

| Flag | Default | Meaning |
|---|---|---|
| `MODE_BTN_PIN` | `BOOT_PIN` | GPIO of the toggle button (active-low, internal pull-up) |
| `MODE_BTN_LONGPRESS_MS` | `1000` | How long to hold before the mode toggles |

> The BOOT button is also a boot strapping pin — holding it **while resetting**
> puts the chip into download mode. That only matters at reset; pressing it
> during normal operation just toggles the scan mode.

---

## Dual-band WiFi scanning (C5)

The ESP32-C5 is dual-band (2.4 + 5 GHz), and the firmware is set up so a **single
build captures 5 GHz in any region** — no per-country flashing. Two things make
that work:

**Per-channel scan type.** 5 GHz regulation splits into DFS and non-DFS bands,
and they must be scanned differently. The firmware picks the scan type from the
channel number:

| Channels | Band | Scan type | Why |
|---|---|---|---|
| 1–14 | 2.4 GHz | **Active** (probe) | Universal; fast. |
| 36–48 | 5 GHz UNII-1 (non-DFS) | **Active** (probe) | Common everywhere. |
| 52–64, 100–144 | 5 GHz UNII-2A/2C (**DFS**) | **Passive** (listen) | DFS is passive-only by law — the radio must hear a beacon, not probe. Heavily used in the **EU**. |
| 149–165 | 5 GHz UNII-3 (non-DFS) | **Active** (probe) | The common **US** home band; probing catches it regardless of regulatory domain. |

Active probes finish fast (~30–120 ms); passive DFS channels dwell ~120 ms
(longer than one ~100 ms beacon interval) so at least one beacon is caught, and
the per-channel cap is widened to ~300 ms for 5 GHz so that dwell completes.

**Regulatory domain.** At init the C5 pins the FCC/`US` domain with a **manual**
policy (`esp_wifi_set_country`, 802.11d off). The reason: the ITU world code
`01` does **not** include the 5 GHz UNII-3 band (channels 149–165), and 802.11d
only adapts after a station *associates* — which a scanner never does — so under
`01` the US networks on 149–165 were never scanned. The `US` domain exposes the
widest common 5 GHz allocation (UNII-1 36–48, UNII-2A/2C 52–144, UNII-3
149–165), a **superset of the EU set** (36–140), so one build reaches every
5 GHz channel a nearby AP might use in either region. `schan/nchan` still cover
2.4 GHz 1–13, keeping the EU-only channels 12/13. A manual policy stops a beacon
advertising a narrower domain from shrinking the channel set mid-scan.

> Why this matters: earlier the C5 active-scanned every channel, which returns
> nothing on DFS (passive-only) and left 5 GHz effectively empty in the EU. The
> classic symptom was **works in the US, not the EU** — US home WiFi favours
> non-DFS UNII-1/UNII-3, while the EU leans on DFS channels that an active-only
> scan can't see.

> Regional note: active-probing 149–165 where those channels are prohibited
> (e.g. the EU) is harmless — there are no APs there — but the driver may log a
> `[WIFI] scan_start FAILED` for them. That line is expected and does not affect
> the rest of the sweep.

The **S3** is 2.4 GHz-only, so none of this applies to it — it just sweeps
channels 1–14 actively.

---

## Flashing the firmware

### Option 1 — Web flasher (easiest, no toolchain)

The fastest way to flash a stock build is the browser-based installer at **<https://pierregode.github.io/HuginnESP/>**. It drives [esptool-js](https://github.com/espressif/esptool-js) **v0.6.0** directly (not esp-web-tools) and serves prebuilt merged images for all three supported boards (Waveshare S3, Waveshare C5, and Seeed XIAO C5).

> **Why not esp-web-tools?** esp-web-tools is pinned to esptool-js v0.5.x, which lacks the ESP32-C5 native-USB (USB-Serial-JTAG) fixes added in esptool-js v0.6.0. The Seeed XIAO ESP32-C5 has **no external UART bridge**, so it can only be web-flashed over that native USB interface. HuginnESP therefore ships a lightweight flasher built on esptool-js v0.6.0 (see [docs/js/flasher.js](docs/js/flasher.js)). The page also includes a built-in **Serial Monitor** for watching the scan stream.

Requirements:
- A Chromium-based browser on desktop (Chrome, Edge, or Opera). Web Serial is required and is not available in Firefox or Safari.
- Page must be served over HTTPS (the GitHub Pages site already is).
- USB-C cable plugged into the **USB** port of the board (the native USB / USB-Serial-JTAG port — not a separate UART port if your board has one).

Steps: open the page → click the **Bind** button for your board (**Waveshare S3**, **Waveshare C5**, or **Seeed XIAO C5**) → pick the serial port → confirm install. Each button flashes a board-specific merged image; the installer refuses to flash if the connected chip doesn't match the board you picked, so choose the right button. Note the two C5 buttons are both ESP32-C5 chips but flash different images (16 MB Waveshare vs 8 MB XIAO) — pick the one matching your physical board.

### Option 2 — Build from source (PlatformIO)

Required for development or custom builds. This is a [PlatformIO](https://platformio.org/) project using [pioarduino](https://github.com/pioarduino/platform-espressif32) — Arduino core 3.x / ESP-IDF 5.3 on the S3 env, 5.5 on the C5 env. The platform is downloaded automatically on first build.

> **Note:** After flashing, the USB port re-enumerates. The combined upload+monitor command handles this automatically.

> **Why pioarduino?** The stock PlatformIO espressif32 platform ships Arduino core 2.x (ESP-IDF 4.4), which has broken BLE on ESP32-S3 and no ESP32-C5 board definitions at all. pioarduino provides the newer cores where both work.

### Option 3 — Build from source (arduino-cli, Seeed XIAO ESP32-C5)

The Seeed XIAO ESP32-C5 uses a **separate pipeline** because its board definition (`XIAO_ESP32C5`) lives only in the official Espressif esp32 Arduino core — pioarduino ships only the C5 *devkit*/Waveshare board. So this target is built with [arduino-cli](https://arduino.github.io/arduino-cli/) instead of PlatformIO.

One-time setup:

```sh
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

Build (assembles a throwaway sketch from `src/` and compiles for `XIAO_ESP32C5`):

```sh
bash scripts/build-xiao.sh
```

The merged web-flasher image is produced by CI; to flash a local build directly, point esptool at the binaries in `build-sketch/HuginnESP/build/esp32.esp32.XIAO_ESP32C5/`. The XIAO C5 has 8 MB flash, so the partition scheme defaults to `default_8MB` (3 MB app) — override with e.g. `XIAO_PARTITION=huge_app bash scripts/build-xiao.sh` if needed. The same firmware (no display) runs as on the Waveshare C5.

---

## The serial protocol

Once flashed, the device starts auto-cycling through scan modes and emits results to USB serial at **460800 baud, 8N1**.

The very first line on every boot is a device announce so a host can tell HuginnESP apart from other ESP32 firmware sharing the same USB bus:

```json
{"device":"HuginnESP","fw":"1.0","board":"esp32-s3","caps":["wifi","ble","display"]}
```

`board` is `esp32-s3` or `esp32-c5`; `caps` lists the compiled-in capabilities (`display` is S3-only, `zigbee` appears only on 802.15.4-capable C5 builds, `gps` appears only in GPS-enabled builds). Hosts that connect to an already-running device can probe with `status` to confirm they're talking to HuginnESP, since no other firmware will respond with the same JSON shape.

### Zigbee / IEEE 802.15.4 (ESP32-C5)

The ESP32-C5's radio also speaks IEEE 802.15.4, so C5 builds add an 802.15.4
sniffer (`-DHUGINN_HAS_ZIGBEE=1`, on by default in the `esp32c5` /
`esp32c5-gps` environments and the XIAO build). It listens promiscuously and
hops across the 2.4 GHz channels (11–26), emitting one JSON line per frame that
carries a source address:

```json
{"type":"ZIGBEE","panid":"0x1A2B","addr":"AABBCCDDEEFF0011","channel":15,"rssi":-70,"lqi":180,"ftype":"beacon","proto":"thread"}
```

- `addr` is the 64-bit extended source address (EUI-64, canonical MSB-first).
  Frames that only carry a 16-bit short address emit `"short":"0x1234"` instead;
  the host keys those by `panid:short`.
- `ftype` is `beacon` / `data` / `cmd`. ACK frames (no addresses) are ignored.
- `proto` classifies the network layer riding on 802.15.4: **`zigbee`**,
  **`thread`**, or **`802.15.4`** (unknown). It's inferred from the beacon
  protocol ID and the payload dispatch above the MAC header — Zigbee's NWK
  header vs Thread's 6LoWPAN dispatch. Link-layer-encrypted data frames are
  opaque, so those report `802.15.4`; the network's beacons still classify it.
  **Matter-over-Thread** is ordinary Thread traffic at this layer, so it reports
  `thread`. (`type` stays `ZIGBEE` as the record/transport tag for backward
  compatibility; `proto` carries the real protocol.)
- `lat`/`lon` are appended when a GPS module has a fix, same as `WIFI` lines.

Because WiFi, BLE and 802.15.4 share the single 2.4 GHz radio on the C5, the
scan cycle parks WiFi and BLE while the Zigbee phase runs (mirroring the
pineapple check). During wardrive a short Zigbee sweep (`ZIGBEE_WARDRIVE_MS`,
default 3 s) runs after each WiFi+BLE round; the `zigbee` serial command
switches to continuous Zigbee-only sniffing, and `stop` returns to auto-cycle.

Tunables (override in `platformio.ini` `build_flags`):

| Flag | Default | Meaning |
|---|---|---|
| `ZIGBEE_CHANNEL_MIN` / `ZIGBEE_CHANNEL_MAX` | `11` / `26` | 802.15.4 channel range to sweep |
| `ZIGBEE_CHANNEL_DWELL_MS` | `300` | Listen time per channel before hopping |
| `ZIGBEE_WARDRIVE_MS` | `3000` | Zigbee sniff time per wardrive cycle |

> Setting `-DHUGINN_HAS_ZIGBEE=1` on a board without an 802.15.4 radio (S3,
> classic ESP32) compiles cleanly but the scanner disables itself at boot with
> a one-line notice — the radio simply isn't there.

After the announce line, the stream is a mix of:

- **Newline-delimited JSON** for raw scan results, one detection per line:
  ```json
    {"type":"WIFI","mac":"AA:BB:CC:DD:EE:FF","ssid":"MyNetwork","rssi":-62,"channel":6,"auth":"WPA2"}
    {"type":"BLE","mac":"11:22:33:44:55:66","name":"AirPods","rssi":-71}
  ```
    In GPS-enabled builds with a valid fix, both `WIFI` and `BLE` JSON lines also include:
    `lat`, `lon`, `speed_kph`, and `speed_mps`.
  (`auth` is one of `Open`, `WEP`, `WPA`, `WPA2`, `WPA/WPA2`, `WPA2-Enterprise`, `WPA3`, `Unknown`.)
- **Plaintext alert blocks** for high-signal events (Flipper Zero, AirTag, skimmer, pineapple/evil-twin), plus `[BOOT]` startup logs and `[CYCLE]` / `[WIFI]` progress logs.

A compact JSON status line is printed **only in response to the `status` command** — it is not streamed continuously:

```json
{"mode":"wifi","wifi_count":12,"ble_count":0}
```

The device also accepts commands on the same serial line (one per `\n`-terminated line):

| Command | Action |
|---|---|
| `scanap` | Start WiFi AP scan |
| `blescan -f` | BLE scan with Flipper/AirTag filter |
| `blescan -a` | BLE scan all devices |
| `capture -skimmer` | Start skimmer detection |
| `pineap` | Start pineapple / evil-twin detection |
| `wardrive` | Tight WiFi+BLE alternation tuned for moving captures (see below) |
| `stop` / `capture -stop` | Stop current scan, resume auto cycle |
| `status` | Print a JSON status line |
| `gps` | Print current GPS fix (`{"gps":"fix","lat":...,"lon":...,"speed_kph":...,"speed_mps":...}` or `{"gps":"no_fix"}`); GPS-enabled builds only |

#### Wardrive mode

In the default auto-cycle each WiFi scan runs for `wifi_scan_duration_ms` (8 s by default), with a pineapple/evil-twin check every eighth scan — each radio is sampled more frequently than before. Engaging `wardrive` switches to a tight 2-phase loop tuned for movement:

| Phase | Default | Effect |
|---|---|---|
| WiFi (`wardrive_wifi_ms`) | 8000 ms | A weighted per-channel sweep, capped at this value. High-traffic channels (2.4 GHz 1/6/11, plus the 5 GHz channels on the C5) are visited first and repeated, then the rest are swept once. On the C5 the scan type is chosen per channel (active for 2.4 GHz + non-DFS 5 GHz, passive for DFS) so 5 GHz is captured in any region — see [Dual-band WiFi scanning (C5)](#dual-band-wifi-scanning-c5). The S3's 2.4 GHz list finishes well inside the cap (~3–4 s); the C5's longer dual-band list can use the full window. WiFi is de-duplicated **within** a sweep (the channel list revisits 1/6/11 and the 5 GHz channels), but each AP is re-emitted **once per cycle** with a fresh GPS fix — so a host that connects mid-session still receives every AP, and a moving capture gets repeated GPS-tagged sightings for triangulation |
| BLE all (`wardrive_ble_ms`) | 1500 ms | Covers all 3 BLE advertising channels with margin; Flipper / AirTag / skimmer detections fire passively from the same stream |

The WiFi phase revisits the busy channels frequently while still covering the whole band, so a moving capture catches in-range APs several times per pass. Lower `wardrive_wifi_ms` (min 1000 ms) for a faster loop with shallower per-channel coverage. Pineapple/evil-twin detection is skipped in wardrive mode because it relies on comparing scans over time; run `stop` and then `pineap` when you want it.

### Runtime configuration

A few internal knobs can be tuned over the same serial line — useful when you want different behavior per integration without rebuilding the firmware. State is held in RAM only (no NVS persistence), so the host should push its preferred values at startup.

```
set <key> <value>     # update a knob
get <key>             # read one knob
get all               # dump all knobs
```

| Key | Type | Range | Effect |
|---|---|---|---|
| `wifi_scan_duration_ms` | uint | 500..600000 | Per-step WiFi scan time in the auto-cycle (and the pineapple scan timeout) |
| `ble_spam_threshold` | uint | 1..10000 | Adverts from one MAC within the spam window before a `BLE Spam detected` alert fires |
| `wardrive_wifi_ms` | uint | 1000..30000 | Ceiling on the per-channel WiFi sweep in `wardrive` mode (default 8000) |
| `wardrive_ble_ms` | uint | 500..30000 | BLE slot length in `wardrive` mode (default 1500 — covers all 3 ad channels with margin) |
| `pineapple_every_n` | uint | 0..1000 | Run periodic pineapple check every N WiFi scans (`0` disables periodic checks; manual `pineap` still works) |
| `skimmer_names` | csv | — | Comma-separated BLE device names treated as suspicious (case-insensitive). Replaces the list, doesn't append |

Every `set`/`get` returns a single JSON status line, e.g.:

```
> set ble_spam_threshold 8
{"ok":true,"key":"ble_spam_threshold","value":8}
> set skimmer_names HC-05,HC-06,JDY-08
{"ok":true,"key":"skimmer_names","value":"HC-05,HC-06,JDY-08"}
> set wifi_scan_duration_ms abc
{"error":"bad value (range 500..600000)"}
> get all
{"ok":true,"key":"wifi_scan_duration_ms","value":8000}
{"ok":true,"key":"ble_spam_threshold","value":8}
{"ok":true,"key":"wardrive_wifi_ms","value":8000}
{"ok":true,"key":"wardrive_ble_ms","value":1500}
{"ok":true,"key":"pineapple_every_n","value":8}
{"ok":true,"key":"skimmer_names","value":"HC-05,HC-06,JDY-08"}
```

Unknown keys, malformed values, and out-of-range numbers all return `{"error":"..."}` and leave the current value untouched. Existing verbs (`scanap`, `blescan -f`, etc.) are unchanged.

**Host integration pattern.** Because the firmware doesn't persist these values, the recommended pattern is:

1. Wait for the `{"device":"HuginnESP",...}` announce line on connect (or after a Huginn reboot).
2. Push your saved keys with `set ...` lines before relying on any specific behavior.
3. Optionally call `get all` afterward to verify the values landed.

[Ragnar](https://github.com/PierreGode/Ragnar) implements exactly this — it persists the values host-side in `shared_config.json` and re-pushes them every time the device announce arrives. Any other host (Home Assistant, a CLI tool, etc.) should follow the same handshake.

## Consuming the stream from your own code

Anything that can open a serial port can consume HuginnESP — Ragnar is just one example. Here's a minimal Python consumer using [pyserial](https://pyserial.readthedocs.io/):

```python
# pip install pyserial
import json
import serial

PORT = "COM8"          # or "/dev/ttyACM0" on Linux/macOS
BAUD = 460800

with serial.Serial(PORT, BAUD, timeout=1) as ser:
    # Optional: ask the device to start a specific scan
    ser.write(b"blescan -a\n")

    for raw in ser:
        line = raw.decode("utf-8", errors="replace").strip()
        if not line:
            continue

        # JSON detections look like {"type":"WIFI",...} or {"type":"BLE",...}
        if line.startswith("{") and line.endswith("}"):
            try:
                evt = json.loads(line)
            except json.JSONDecodeError:
                print("raw:", line)
                continue

            if evt.get("type") == "WIFI":
                print(f"WIFI  {evt['ssid']!r:30} {evt['mac']}  ch{evt['channel']:>2}  {evt['rssi']} dBm  {evt['auth']}")
            elif evt.get("type") == "BLE":
                print(f"BLE   {evt.get('name','') or '<unnamed>':30} {evt['mac']}  {evt['rssi']} dBm")
            else:
                print("status:", evt)
        else:
            # Plaintext alert / boot log
            print("log:", line)
```

That's the entire integration surface — open the port, read lines, parse JSON. Ragnar's wardriving engine does the same thing in `wardriving.py → _parse_serial_line()`; you can replace it with anything (Home Assistant, MQTT bridge, a CLI logger, etc.).

---

## Project Structure

```
src/
├── main.cpp            # Entry point, FreeRTOS task creation
├── config.h            # Constants and configuration
├── wifi_scanner.h/cpp  # WiFi scanning & pineapple detection
├── ble_scanner.h/cpp   # BLE scanning, Flipper/AirTag/skimmer/spam detection
├── serial_cmd.h/cpp    # Serial command parser
├── runtime_config.h/cpp # `set`/`get` knobs (RAM-only, host-pushed)
├── scan_cycle.h/cpp    # Automatic scan rotation
├── gps_reader.h/cpp    # NMEA GPS reader — compiled in only with HUGINN_HAS_GPS=1
└── display_manager.h/cpp # 480×480 touch display UI (S3 only)
docs/                   # Web flasher (GitHub Pages site)
.github/workflows/      # CI: builds firmware and publishes the flasher
```

## Architecture

```
┌─────────────────────────────────────────────────┐
│       ESP32-S3 / ESP32-C5                       │
│                                                 │
│  ┌─────────┐  ┌──────────┐  ┌─────────┐        │
│  │ WiFi    │  │ BLE      │  │ Display │        │
│  │ Scanner │  │ Scanner  │  │ Manager │        │
│  │ (task)  │  │ (task)   │  │ (S3)    │        │
│  └────┬────┘  └────┬─────┘  └────┬────┘        │
│       │            │             │              │
│       ▼            ▼             │              │
│  ┌─────────────────────┐        │  ┌─────────┐ │
│  │   Serial Output     │◄───────┘  │ GPS     │ │
│  │   (460800 baud)     │◄──────────│ Reader  │ │
│  └─────────┬───────────┘           │ (task)  │ │
│            │                       └────┬────┘ │
│  ┌─────────▼───────────┐               │      │
│  │  Serial Command     │          UART to     │
│  │  Parser (incoming)  │          NMEA module │
│  └─────────────────────┘                      │
└──────────────┬────────────────────────────────┘
               │ USB Serial (JSON lines)
               ▼
┌─────────────────────────────────────────────────┐
│  Any host: Ragnar (Raspberry Pi),               │
│  a Python script, Home Assistant, ...           │
└─────────────────────────────────────────────────┘
```

## License

MIT
