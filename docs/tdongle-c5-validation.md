# T-Dongle-C5 validation

Test hardware: ESP32-C5HR8 rev 1.0, 16 MB flash, 8 MB PSRAM, native USB.

- `pio run -e lilygo-tdongle-c5`: passed.
- `pio run -e esp32c5`: passed (existing headless target).
- New board JSON, web-flasher manifests, and workflow YAML parse successfully.
- Full pre-existing firmware backup verified against flash before testing.
- Final merged test image uses DIO at 80 MHz, with bootloader at 0x2000.
- Firmware write verified by esptool.
- Runtime reports the correct board and all 8 MB PSRAM; heap after init: 120020 bytes.
- Live USB capture: 226 WiFi and 144 BLE records in a 25-second sample,
  with no panic, assertion, brownout, or reset messages in that sample.
- Switching through `capture -skimmer` and `wardrive` returns the expected status;
  wardrive resumes emitting WiFi and BLE records.
- Physical tester confirms text is visible with the final software-SPI display.
- Physical tester confirms readable layout/colors, updating counts, and BOOT
  long-press switching between wardrive and skimmer modes.
- Live Ragnar ingestion verified over native USB on a Pi 5: companion remains
  identified as Huginn in wardrive mode; WiFi records increase from 33 to 52
  (both 2.4/5 GHz), and BLE records from 83 to 112 during a 25-second check.
  With a power-bank setup, adding the dongle alongside an RTL-SDR and an external
  USB WiFi adapter triggered shared USB over-current disconnects. Removing the
  external WiFi adapter restored ingestion; over-current counters remained
  unchanged throughout that check. This is a short functional test, not a soak.

The first test used a QIO merged boot header and failed in ROM; the deployment
recipe now preserves DIO, matching the generated C5 bootloader. Hardware SPI
produced a blank display in the first working radio build; explicit software SPI
works on this unit and is used for the small, once-per-second text-only display.

Remaining checks before marking support fully validated:

- Longer scan soak.
- Zigbee frame capture against a known test transmitter. Zigbee is deliberately
  not added to the default WiFi/BLE wardrive cycle.

An Espressif startup warning about allocation of an MSPI dummy cacheline appears
before Arduino setup. PSRAM is subsequently added to the heap and reported as
8388608 bytes. This warning has not been resolved in this board-support change.
