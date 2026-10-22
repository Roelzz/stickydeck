# StickyDeck

Turn the Seeed **reTerminal Sticky** (ESP32-S3 + 3.97" ePaper + GT911 touch) into a
**FreeTouchDeck-style BLE macro deck**: a touchscreen keypad that emulates a BLE
HID keyboard and fires macros, hotkeys, text, media keys, and app launches on a
paired computer.

## Hardware

| Subsystem | Detail |
|---|---|
| MCU | ESP32-S3R8, dual-core LX7 @ 240 MHz, 8 MB PSRAM |
| Flash | 32 MB QSPI |
| Display | 3.97" B&W ePaper, 800×480, 4-level gray, SSD1677 |
| Touch | GT911 capacitive, I²C |
| Input | AI/Power, Up, Down buttons + buzzer |
| Wireless | Wi-Fi 2.4 GHz + BLE 5.0 |
| USB | USB-C (UART debug only — no native USB-HID) |

## Concept

```
tap a key → fire BLE HID action immediately → buzzer feedback
         → partial ePaper refresh of a "last key" highlight
```

ePaper retains the button grid at zero power, so the deck can deep-sleep and
still look live.

## Architecture

- `devices/` — GT911 touch (raw taps), ePaper, buttons, buzzer, BLE HID
- `app/` — event loop, action engine, page navigation, sleep
- `pages/` — button-grid renderer (monochrome + partial refresh)
- `config/` — JSON config (pages, buttons, actions) from microSD / flash

## Toolchain

ESP-IDF v5.4, target `esp32s3`.

See `docs/plan.md` for the full implementation plan.
