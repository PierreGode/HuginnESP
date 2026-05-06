# Ragnar ESP32-S3 Scanner Firmware

Custom firmware for **Waveshare ESP32-S3 Smart 86 Box** (4" 480×480 RGB touch display, ESP32-S3, WiFi 2.4 GHz + BLE 5).

Designed to integrate with Ragnar's wardriving engine via USB serial (115 200 baud).

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
| Board | Waveshare ESP32-S3 Smart 86 Box |
| Display | 4" IPS, 480×480, RGB interface |
| Touch | GT911 capacitive, 5-point, I2C |
| MCU | ESP32-S3 (dual-core, WiFi + BLE 5) |
| Flash | 16 MB |
| PSRAM | 8 MB (OPI) |

## Building

This is a [PlatformIO](https://platformio.org/) project.

```bash
# Build
pio run

# Upload
pio run --target upload

# Monitor serial
pio device monitor -b 115200
```

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
│            ESP32-S3 Smart 86 Box         │
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

See repository for license information.
