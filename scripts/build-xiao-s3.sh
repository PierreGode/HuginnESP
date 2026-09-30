#!/usr/bin/env bash
# =====================================================================
#  build-xiao-s3.sh — compile HuginnESP for the Seeed XIAO ESP32-S3
#
#  Companion to build-xiao.sh (XIAO ESP32-C5). Use this arduino-cli path
#  if you prefer the official Espressif board definition
#  (FQBN esp32:esp32:XIAO_ESP32S3) over the PlatformIO [env:xiao-s3].
#  Either route produces the same headless S3 firmware; PlatformIO is
#  simpler if you already run `pio` (`pio run -e xiao-s3`).
#
#  Headless S3 build: WiFi 2.4 GHz + BLE, no display, no Zigbee (S3 has no
#  802.15.4 radio). USBMode=hwcdc matches the platformio env's USB_MODE=1
#  (S3 USB-Serial-JTAG); enumerates as VID 303a for the Ragnar host.
#
#  Usage:   bash scripts/build-xiao-s3.sh
#  Output:  build-sketch-s3/HuginnESP/build/esp32.esp32.XIAO_ESP32S3/HuginnESP.ino.*
#
#  Prereqs: arduino-cli on PATH and the esp32 core installed:
#    arduino-cli config init
#    arduino-cli config add board_manager.additional_urls \
#      https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json
#    arduino-cli core update-index
#    arduino-cli core install esp32:esp32
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

SKETCH_DIR="build-sketch-s3/HuginnESP"

# XIAO ESP32-S3 has 8 MB flash; default_8MB gives ~3 MB app / 1.5 MB SPIFFS,
# plenty for Bluedroid BLE + WiFi. USBMode=hwcdc = USB-Serial-JTAG (matches
# ARDUINO_USB_MODE=1 in the platformio env). CDCOnBoot=default = "USB CDC On
# Boot: Enabled" (Serial → USB); note the esp32 core inverts this token —
# CDCOnBoot=cdc means DISABLED, which would drop Serial onto UART0 (GPIO43/44,
# the GPS pins). PSRAM=opi matches the XIAO S3's octal PSRAM.
PARTITION="${XIAO_S3_PARTITION:-default_8MB}"
FQBN="esp32:esp32:XIAO_ESP32S3:PartitionScheme=${PARTITION},USBMode=hwcdc,CDCOnBoot=default,PSRAM=opi"

echo "==> Assembling Arduino sketch at $SKETCH_DIR from src/"
rm -rf "$SKETCH_DIR"
mkdir -p "$SKETCH_DIR/src"
cp -r src/* "$SKETCH_DIR/src/"

cat > "$SKETCH_DIR/HuginnESP.ino" <<'INO'
// HuginnESP arduino-cli wrapper for the Seeed XIAO ESP32-S3.
// setup() and loop() are defined in src/main.cpp.
INO

# Headless S3: reuse the S3 chip paths, compile the display out. Use
# compiler.cpp.extra_flags so the board's own USB-CDC build defines are kept.
EXTRA_FLAGS="-DHUGINN_BOARD_S3=1 -DHUGINN_BOARD_XIAO_S3=1 -DHUGINN_HAS_DISPLAY=0 -DCORE_DEBUG_LEVEL=3"

# Set XIAO_S3_GPS=1 to enable the L76K GNSS on the D7/D6 UART pads. Pins default
# to RX=GPIO44 / TX=GPIO43 via config.h (HUGINN_BOARD_XIAO_S3); override with
# GPS_RX_PIN / GPS_TX_PIN if you wired it elsewhere.
if [ "${XIAO_S3_GPS:-0}" = "1" ]; then
  EXTRA_FLAGS="$EXTRA_FLAGS -DHUGINN_HAS_GPS=1"
  echo "==> GPS enabled (UART1, RX=GPIO44/D7, TX=GPIO43/D6)"
fi

echo "==> Compiling for $FQBN"
arduino-cli compile \
  --fqbn "$FQBN" \
  --build-property "compiler.cpp.extra_flags=$EXTRA_FLAGS" \
  --export-binaries \
  "$SKETCH_DIR"

echo "==> Done. Binaries in $SKETCH_DIR/build/esp32.esp32.XIAO_ESP32S3/"
