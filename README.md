English | [한국어](README.ko.md)

# Wireless Music Page Turner Pedal

A BLE foot pedal for turning pages in sheet music apps. Step on it and the page turns, no cables to your tablet or phone.

Built around a Waveshare ESP32-S3-Zero devkit on a custom carrier board. The pedal shows up as a standard BLE HID keyboard, so it works with forScore, piaScore, and any other app that responds to keyboard shortcuts.

## Status

Hardware design is finalized and the first PCB/PCBA run is in progress. Firmware covers footswitch input (debouncing plus key-mapping profiles), BLE HID over NimBLE, battery monitoring with low-voltage cutoff, the connect switch, and the status LED, but none of it has been validated against real hardware yet.

## How it works

- Two latching footswitches wired to GPIO4 (left / "previous") and GPIO5 (right / "next"). Since the switches are latching rather than momentary, the firmware tracks GPIO edges rather than levels so each step registers as a single press.
- A 4-bit DIP switch selects a key-mapping profile at boot, so the same pedal can send arrow keys, Page Up/Down, or presentation-style Backspace/Space depending on which app it's paired with.
- A dedicated switch triggers BLE pairing mode. The devkit's onboard BOOT/RESET buttons still handle flashing and reset.
- Runs off a single 18650 cell charged through a TP4056 module, with battery voltage sensed via ADC for low-voltage cutoff.

### GPIO map

| GPIO | Function |
|------|----------|
| GP4  | Left footswitch ("previous") |
| GP5  | Right footswitch ("next") |
| GP6–GP9 | DIP switch (4-bit key-mapping profile select) |
| GP10 | BLE pairing switch |
| GP11 | Battery voltage sense (ADC) |
| GP12 | Status LED |
| GP13 | Debug header only |

GPIO21 and GPIO33–37 are reserved by the devkit itself (onboard RGB LED and PSRAM) and are not used on the carrier board.

### Key-mapping profiles

| # | DIP (GP9 GP8 GP7 GP6) | Previous | Next | Target |
|---|---|---|---|---|
| 0 | `0000` (default) | Left Arrow | Right Arrow | Most sheet music apps (forScore, etc.) |
| 1 | `0001` | Up Arrow | Down Arrow | Vertical-scroll viewers |
| 2 | `0010` | Page Up | Page Down | Desktop PDF readers |
| 3 | `0011` | Backspace | Space | Presentation "clicker" style (PowerPoint/Keynote) |
| 4 | `0100` | Backspace | Enter | Viewers that page forward on Enter |

Unused combinations fall back to profile 0.

## Firmware

Built with ESP-IDF (not Arduino/PlatformIO), targeting `esp32s3`. BLE HID uses the `esp_hid` component over the NimBLE backend, BLE-only (no Bluetooth Classic).

```
idf.py set-target esp32s3   # already the default via sdkconfig.defaults
idf.py build
idf.py -p <PORT> flash monitor
```

## Hardware source

Schematic, Gerber files, and 3D model are in [`hardware/`](hardware/). BOM will follow once the build is finished and verified. PCB fabrication and assembly for this project are sponsored by [PCBWay](https://www.pcbway.com/).

## License

TBD. Will be added alongside the hardware files at release.
