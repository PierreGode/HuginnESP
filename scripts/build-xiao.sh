#!/usr/bin/env bash
# =====================================================================
#  build-xiao.sh — compile SkimGuard C5 for the Seeed XIAO ESP32-C5
#
#  The XIAO ESP32-C5 board definition lives only in the official
#  Espressif esp32 Arduino core (FQBN esp32:esp32:XIAO_ESP32C5), so this
#  target is built with arduino-cli rather than the PlatformIO/pioarduino
#  pipeline used for the ESP32-C5 dev board.
#
#  arduino-cli needs a sketch directory whose name matches a top-level
#  .ino and compiles every .cpp in the sketch root plus the src/ subfolder
#  recursively. The sources already live in src/ with setup()/loop()
#  in src/main.cpp, so we assemble a throwaway sketch from that single
#  source of truth (no committed duplication) and compile it.
#
#  Usage:   bash scripts/build-xiao.sh
#  Output:  build-sketch/SkimGuardC5/build/esp32.esp32.XIAO_ESP32C5/SkimGuardC5.ino.*
#
#  Prereqs: arduino-cli on PATH and the esp32 core installed:
#    arduino-cli config init
#    arduino-cli config add board_manager.additional_urls \
#      https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json
#    arduino-cli core update-index
#    arduino-cli core install esp32:esp32
# =====================================================================
set -euo pipefail

# Repo root = parent of this script's dir, regardless of where it's invoked.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

SKETCH_DIR="build-sketch/SkimGuardC5"

# Partition scheme — overridable for tuning. The XIAO ESP32-C5 has 8 MB
# flash, so default_8MB (3 MB app / 1.5 MB SPIFFS) is the board's native
# default and gives ample room for Bluedroid BLE + WiFi (firmware is ~1.4 MB).
PARTITION="${XIAO_PARTITION:-default_8MB}"
FQBN="esp32:esp32:XIAO_ESP32C5:PartitionScheme=${PARTITION}"

echo "==> Assembling Arduino sketch at $SKETCH_DIR from src/"
rm -rf "$SKETCH_DIR"
mkdir -p "$SKETCH_DIR/src"
cp -r src/* "$SKETCH_DIR/src/"

# Stub .ino — setup()/loop() come from src/main.cpp; the esp32 core's
# main() calls them. arduino-cli auto-prepends #include <Arduino.h>.
cat > "$SKETCH_DIR/SkimGuardC5.ino" <<'INO'
// SkimGuard C5 arduino-cli wrapper for the Seeed XIAO ESP32-C5.
// setup() and loop() are defined in src/main.cpp.
INO

echo "==> Compiling for $FQBN"
# Headless XIAO C5 build for continuous BLE skimmer detection.
# Use compiler.cpp.extra_flags (empty by default) — NOT build.extra_flags,
# which carries the board's USB-CDC defines. BOARD_HAS_PSRAM is intentionally
# left unset: this firmware fits in internal SRAM without PSRAM requirements.
arduino-cli compile \
  --fqbn "$FQBN" \
  --build-property "compiler.cpp.extra_flags=-DHUGINN_BOARD_C5=1 -DHUGINN_BOARD_XIAO_C5=1 -DHUGINN_HAS_DISPLAY=0 -DHUGINN_HAS_GPS=0 -DHUGINN_HAS_SKIMMER_LED=1 -DHUGINN_HAS_MODE_BUTTON=0 -DCORE_DEBUG_LEVEL=0" \
  --export-binaries \
  "$SKETCH_DIR"

echo "==> Done. Binaries in $SKETCH_DIR/build/esp32.esp32.XIAO_ESP32C5/"
