# HuginnESP

WiFi & BLE security scanner firmware for ESP32. The device performs the radio scanning and pushes a live stream of detections out over USB serial — any host can consume the stream. The reference consumer is [Ragnar](https://github.com/PierreGode/Ragnar)'s wardriving engine, but the protocol is plain newline-delimited JSON so anything that can read a serial port will work.

---

## Supported devices

| Board | MCU | Radio | Display |
|---|---|---|---|
| **Waveshare ESP32-S3-Touch-LCD-4B** | ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB PSRAM) | WiFi 2.4 GHz + BLE 5 | 4" 480×480 RGB touch (GT911) |
| **Waveshare ESP32-C5-WIFI6-KIT** | ESP32-C5-WROOM-1 N16R4 (16 MB flash, 4 MB PSRAM, RISC-V) | Dual-band WiFi 6 (2.4 / 5 GHz) + BLE 5 | none (headless) |

Both boards run the same firmware behavior; the C5 build skips display code (`HUGINN_HAS_DISPLAY=0`).

## Features

| Feature | Description |
|---|---|
| **WiFi Scan** | Scan WiFi networks (SSID, BSSID, RSSI, channel, security) — 2.4 GHz on S3, dual-band on C5 |
| **BLE Scan** | Scan BLE devices (MAC, name, RSSI, device type) |
| **Flipper Zero Detection** | Identify Flipper Zero devices via BLE advertisement data |
| **AirTag Detection** | Identify Apple AirTags via BLE manufacturer data |
| **BLE Spam Detection** | Detect BLE advertising spam attacks |
| **Skimmer Detection** | Identify potential skimmer devices (HC-05/HC-06 BLE modules) |
| **Evil Twin / Pineapple** | Identify duplicate SSIDs with different BSSIDs |
| **Touch Display** | Live status, touch buttons, alert panel with color coding (S3 only) |
| **Session Tally** | Display-side running totals (unique WiFi BSSIDs, BLE / Flipper / AirTag / skimmer MACs) since power-on; resets on reboot, S3 only |
| **Auto Scan Cycle** | Automatic rotation through all scan modes |

---

## Flashing the firmware

### Option 1 — Web flasher (easiest, no toolchain)

The fastest way to flash a stock build is the browser-based installer at **<https://pierregode.github.io/HuginnESP/>**. It is built on [ESP Web Tools](https://esphome.github.io/esp-web-tools/) and serves prebuilt merged images for both supported boards (`esp32s3box`, `esp32c5`)

Requirements:
- A Chromium-based browser on desktop (Chrome, Edge, or Opera). Web Serial is required and is not available in Firefox or Safari.
- Page must be served over HTTPS (the GitHub Pages site already is).
- USB-C cable plugged into the **USB** port of the board (the native USB / USB-Serial-JTAG port — not a separate UART port if your board has one).

Steps: open the page → "Bind the Raven" → pick the serial port → the installer auto-detects the chip family and flashes the matching image.

### Option 2 — Build from source (PlatformIO)

Required for development or custom builds. This is a [PlatformIO](https://platformio.org/) project using [pioarduino](https://github.com/pioarduino/platform-espressif32) — Arduino core 3.x / ESP-IDF 5.3 on the S3 env, 5.5 on the C5 env. The platform is downloaded automatically on first build.

> **Note:** After flashing, the USB port re-enumerates. The combined upload+monitor command handles this automatically.

> **Why pioarduino?** The stock PlatformIO espressif32 platform ships Arduino core 2.x (ESP-IDF 4.4), which has broken BLE on ESP32-S3 and no ESP32-C5 board definitions at all. pioarduino provides the newer cores where both work.

---

## The serial protocol

Once flashed, the device starts auto-cycling through scan modes and emits results to USB serial at **115200 baud, 8N1**.

The very first line on every boot is a device announce so a host can tell HuginnESP apart from other ESP32 firmware sharing the same USB bus:

```json
{"device":"HuginnESP","fw":"1.0","board":"esp32-s3","caps":["wifi","ble","display"]}
```

`board` is `esp32-s3` or `esp32-c5`; `caps` lists the compiled-in capabilities (the `display` cap is S3-only). Hosts that connect to an already-running device can probe with `status` to confirm they're talking to HuginnESP, since no other firmware will respond with the same JSON shape.

After the announce line, the stream is a mix of:

- **Newline-delimited JSON** for raw scan results, one detection per line:
  ```json
  {"type":"WIFI","mac":"AA:BB:CC:DD:EE:FF","ssid":"MyNetwork","rssi":-62,"channel":6,"auth":"WPA2_PSK"}
  {"type":"BLE","mac":"11:22:33:44:55:66","name":"AirPods","rssi":-71}
  {"mode":"wifi","wifi_count":12,"ble_count":0}
  ```
- **Plaintext alert blocks** for high-signal events (Flipper Zero, AirTag, skimmer, pineapple/evil-twin), and `[BOOT]`-prefixed startup logs.

The device also accepts commands on the same serial line (one per `\n`-terminated line):

| Command | Action |
|---|---|
| `scanap` | Start WiFi AP scan |
| `blescan -f` | BLE scan with Flipper/AirTag filter |
| `blescan -a` | BLE scan all devices |
| `capture -skimmer` | Start skimmer detection |
| `pineap` | Start pineapple / evil-twin detection |
| `stop` / `capture -stop` | Stop current scan, resume auto cycle |
| `status` | Print a JSON status line |

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
{"ok":true,"key":"wifi_scan_duration_ms","value":15000}
{"ok":true,"key":"ble_spam_threshold","value":8}
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
BAUD = 115200

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
└── display_manager.h/cpp # 480×480 touch display UI (S3 only)
docs/                   # Web flasher (GitHub Pages site)
.github/workflows/      # CI: builds firmware and publishes the flasher
```

## Architecture

```
┌──────────────────────────────────────────┐
│       ESP32-S3 / ESP32-C5                │
│                                          │
│  ┌─────────┐  ┌──────────┐  ┌─────────┐ │
│  │ WiFi    │  │ BLE      │  │ Display │ │
│  │ Scanner │  │ Scanner  │  │ Manager │ │
│  │ (task)  │  │ (task)   │  │ (S3)    │ │
│  └────┬────┘  └────┬─────┘  └────┬────┘ │
│       │            │             │       │
│       ▼            ▼             │       │
│  ┌─────────────────────┐        │       │
│  │   Serial Output     │◄───────┘       │
│  │   (115200 baud)     │                │
│  └─────────┬───────────┘                │
│            │                             │
│  ┌─────────▼───────────┐                │
│  │  Serial Command     │                │
│  │  Parser (incoming)  │                │
│  └─────────────────────┘                │
└──────────────┬───────────────────────────┘
               │ USB Serial (JSON lines)
               ▼
┌──────────────────────────────────────────┐
│  Any host: Ragnar (Raspberry Pi),        │
│  a Python script, Home Assistant, ...    │
└──────────────────────────────────────────┘
```

## License

MIT
