# HuginnESP

<img src="docs/img/huginn.png" alt="Huginn" width="160" align="right">

Wardriving firmware for ESP32-C5 and ESP32-S3 boards. A Huginn scans Wi-Fi
(2.4 and 5 GHz on the C5), Bluetooth LE and, on the C5, Zigbee / Thread
(IEEE 802.15.4), flags Flipper Zero, AirTag-style trackers, card skimmers and
evil-twin access points, and streams what it hears as newline-delimited JSON —
over USB to [Ragnar](https://github.com/PierreGode/Ragnar), and over Bluetooth
to the HuginnESP iPhone app.

## Install

Flash it from the browser: **<https://pierregode.github.io/HuginnESP/>**
(Chrome or Edge on a computer, USB cable to the board). Pick your board —
Waveshare ESP32-C5, Seeed XIAO ESP32-C5, ESP32-C5-WROOM-1 / 1U devkits,
LilyGO T-Dongle C5 or Waveshare ESP32-S3 — and the page installs the latest
build.

After that first install, the HuginnESP app updates the firmware over
Bluetooth: Settings shows when a newer build is out.

## Use

- **With the phone app:** the app finds Huginns in range, shows what they hear
  live, records drives with the phone's GPS and uploads them to WiGLE,
  WDGWars or Wardrift. [Support](docs/ios/SUPPORT.md) ·
  [Privacy policy](docs/ios/PRIVACY.md)
- **With Ragnar:** plug the Huginn into Ragnar's USB; Ragnar adds its finds to
  its own wardriving, placed with Ragnar's GPS.

Positions come from the host: the phone's GPS, or Ragnar's.

## This repository

HuginnESP is developed in a private repository. This one publishes the web
flasher and hosts the app's support pages:

- the private pipeline builds every board, then starts this repo's
  [`deploy-flasher.yml`](.github/workflows/deploy-flasher.yml), which downloads
  that build and publishes it to GitHub Pages;
- [`docs/ios/`](docs/ios) holds the app's support and privacy pages.

The firmware source as it was when development moved is in this repository's
history (commit `848197a` and earlier); the flasher always carries the current
firmware.

Bugs and ideas: [issues](https://github.com/PierreGode/HuginnESP/issues).
