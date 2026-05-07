# HuginnESP

WiFi & BLE security scanner firmware for the **Waveshare ESP32-S3-Touch-LCD-4B** (ESP32-S3-WROOM-1-N16R8, 4" 480×480 RGB touch display, WiFi 2.4 GHz + BLE 5).

Designed to integrate with Ragnar's wardriving engine via USB serial (115200 baud).

---

## Features

| Feature | Description |
|---|---|
| **WiFi Scan** | Scan 2.4 GHz networks (SSID, BSSID, RSSI, channel, security) |
| **BLE Scan** | Scan BLE devices (MAC, name, RSSI, device type) |
| **Flipper Zero Detection** | Identify Flipper Zero devices via BLE advertisement data |
| **AirTag Detection** | Identify Apple AirTags via BLE manufacturer data |
| **BLE Spam Detection** | Detect BLE advertising spam attacks |
| **Skimmer Detection** | Identify potential skimmer devices (HC-05/HC-06 BLE modules) |
| **Evil Twin / Pineapple** | Identify duplicate SSIDs with different BSSIDs |
| **Touch Display** | Live status, touch buttons, alert panel with color coding |
| **Auto Scan Cycle** | Automatic rotation through all scan modes |

## Hardware

| Parameter | Value |
|---|---|
| Board | Waveshare ESP32-S3-Touch-LCD-4B |
| Display | 4" IPS, 480×480, RGB interface |
| Touch | GT911 capacitive, 5-point, I2C |
| MCU | ESP32-S3 (dual-core, WiFi + BLE 5) |
| Flash | 16 MB |
| PSRAM | 8 MB (OPI) |

## Prerequisites

- [VS Code](https://code.visualstudio.com/) with the [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
- USB-C cable connected to the **USB** port (native USB Serial/JTAG — not the UART port)

## Build & Flash

This is a [PlatformIO](https://platformio.org/) project using [pioarduino](https://github.com/pioarduino/platform-espressif32) (Arduino core 3.x / ESP-IDF 5.3). The platform is downloaded automatically on first build.

```bash
# Clone
git clone https://github.com/YOUR_USER/HuginnESP.git
cd HuginnESP

# Build
pio run

# Upload (replace COM8 with your port)
pio run --target upload --upload-port COM8

# Monitor serial output
pio device monitor --port COM8 --baud 115200
```

Or build, upload, and monitor in one command:

```bash
pio run --target upload --upload-port COM8 --target monitor
```

> **Note:** After flashing, the USB port re-enumerates. The combined upload+monitor command handles this automatically.

> **Why pioarduino?** The stock PlatformIO espressif32 platform ships Arduino core 2.x (ESP-IDF 4.4), which has broken BLE on ESP32-S3. pioarduino provides Arduino core 3.x with ESP-IDF 5.3 where BLE works correctly.

## Serial Commands (from Ragnar)

| Command | Action |
|---|---|
| `scanap` | Start WiFi AP scan |
| `blescan -f` | BLE scan with Flipper/AirTag filter |
| `blescan -a` | BLE scan all devices |
| `capture -skimmer` | Start skimmer detection |
| `pineap` | Start pineapple / evil twin detection |
| `stop` | Stop current scan, resume auto cycle |
| `capture -stop` | Stop capture mode |
| `status` | Print JSON status |

## Project Structure

```
src/
├── main.cpp            # Entry point, FreeRTOS task creation
├── config.h            # Constants and configuration
├── wifi_scanner.h/cpp  # WiFi scanning & pineapple detection
├── ble_scanner.h/cpp   # BLE scanning, Flipper/AirTag/skimmer/spam detection
├── serial_cmd.h/cpp    # Serial command parser
├── scan_cycle.h/cpp    # Automatic scan rotation
└── display_manager.h/cpp # 480×480 touch display UI
```

## Architecture

```
┌──────────────────────────────────────────┐
│       ESP32-S3-Touch-LCD-4B              │
│                                          │
│  ┌─────────┐  ┌──────────┐  ┌─────────┐ │
│  │ WiFi    │  │ BLE      │  │ Display │ │
│  │ Scanner │  │ Scanner  │  │ Manager │ │
│  │ (task)  │  │ (task)   │  │ (task)  │ │
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
               │ USB Serial
               ▼
┌──────────────────────────────────────────┐
│  Ragnar (Raspberry Pi)                   │
│  wardriving.py → _parse_serial_line()    │
└──────────────────────────────────────────┘
```

## License

MIT
