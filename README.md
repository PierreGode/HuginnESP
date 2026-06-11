# SkimGuard C5

SkimGuard C5 is a dedicated BLE skimmer detector firmware for ESP32-C5 hardware.

This rebuild removes wardriving and serial command streaming entirely. The firmware now runs continuous BLE scanning all the time and focuses only on suspicious skimmer-class BLE modules.

## Supported hardware

- ESP32-C5 Dev Board / Waveshare ESP32-C5-WIFI6-KIT (16 MB flash)
- Seeed XIAO ESP32-C5 (8 MB flash)

## Runtime behavior

- BLE scan starts at boot and runs continuously.
- Detection is local on the device, no host protocol required.
- Onboard RGB LED is used as proximity feedback:
- Red/white blink pattern means skimmer signature detected.
- Faster blinking means stronger RSSI (closer source).
- No wardrive mode.
- No serial command parser loop.

## Detection strategy

SkimGuard C5 classifies suspicious devices by BLE advertised name fingerprints.

Default list includes common skimmer-adjacent module families:

- HC-05/HC-06/HC-08 variants
- BT05/BT06
- CC41
- JDY module family
- Linvor / MLT-BT05 style names

Matching is normalized and tolerant to separators/casing so variants like HC-05, HC05, and hc_05 are all recognized.

## Build

### PlatformIO (ESP32-C5 Dev Board)

```bash
pio run -e esp32c5-dev
```

### Arduino CLI (Seeed XIAO ESP32-C5)

```bash
bash scripts/build-xiao.sh
```

## Web flasher

Web flasher in docs now targets only the two supported C5 profiles:

- ESP32-C5 Dev Board profile
- Seeed XIAO ESP32-C5 profile

The GitHub Actions deploy workflow builds both binaries and publishes the flasher site with C5-only manifests.

## Notes

- This project does not use eFuse operations.
- Flashing is regular firmware write only.
