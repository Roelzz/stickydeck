# StickyDeck

Turn the Seeed **reTerminal Sticky** (ESP32-S3 + 3.97" ePaper + GT911 touch) into a
**FreeTouchDeck-style BLE macro deck**: a touchscreen keypad that emulates a BLE
HID keyboard and fires hotkeys, typed text, and media keys on a paired computer.

## Status

Working v1+. On boot it loads its config (web-saved NVS > microSD `deck.json` >
built-in default), renders a monochrome button grid + status bar on the ePaper,
advertises as a BLE HID keyboard, fires actions on tap, and hosts a **web
configurator** on Wi-Fi.

```
tap a button → fire BLE HID action immediately → buzzer (if enabled)
            → partial ePaper refresh to highlight the key
Up/Down buttons or left/right swipe → switch pages
AI short-press → refresh the page   AI long-press → deep sleep
Status bar: battery % + BLE + Wi-Fi indicators
Tap the header bar → toggle the Wi-Fi config AP on/off
Wi-Fi AP "StickyDeck-Config" → browser editor at http://192.168.4.1/
```

## Features

- BLE HID keyboard + consumer (media) keys
- Touch buttons with built-in icons; multi-page navigation
- Battery level reported to the host over BLE (macOS shows it in the Bluetooth menu)
- Status bar: battery %, BLE + Wi-Fi state; tap the header to toggle Wi-Fi
- Web configurator — Wi-Fi AP + visual browser editor
- Auto-sleep after N idle minutes (configurable)
- Deep sleep (AI long-press) / wake (AI press), ePaper keeps the last image
- Config-error indicator (`!`) if the user JSON is invalid

## Hardware

| Subsystem | Detail |
|---|---|
| MCU | ESP32-S3R8, dual-core LX7 @ 240 MHz, 8 MB PSRAM |
| Flash | 32 MB QSPI |
| Display | 3.97" B&W ePaper, 800×480, 4-level gray, SSD1677 |
| Touch | GT911 capacitive, I²C |
| Input | AI/Power, Up, Down buttons + buzzer |
| Wireless | BLE 5.0 (HID keyboard + consumer control) |
| USB | USB-C (UART debug only — no native USB-HID) |

## Layout

```
main/
├── main.cpp            # entry: deck_app_run()
├── app/                # event queue (AppEventMessage)
├── board/              # power + shared buses
├── devices/            # sticky_display/buzzer/buttons/touch/sdcard/power/battery
├── ui/                 # Canvas + 5x7 font
├── deck/
│   ├── deck_config     # JSON config (NVS > SD > default)
│   ├── deck_actions    # action execution + HID key lookups
│   ├── deck_render     # grid render + hit-test + status bar
│   ├── deck_app        # init + event loop
│   └── webserver       # Wi-Fi AP + HTTP config editor
├── partitions.csv      # 4MB app partition (32MB flash)
components/
├── ble_hid/            # BLE HID keyboard + media device
├── seeed_epaper/       # SSD1677 ePaper driver
├── gt911/              # capacitive touch driver
└── button/             # iot-button (button events)
```

## Configuration

### Web editor (easiest)

1. On your phone/laptop, join the Wi-Fi network **`StickyDeck-Config`**
   (password **`stickydeck`**).
2. Browse to **http://192.168.4.1/**.
3. Edit pages, buttons, and settings in the visual editor, then **Save & reboot**.
   The config is stored in flash (NVS) and survives reboots.

### microSD (alternative)

Place a `deck.json` on the microSD card (overrides the built-in default; a
web-saved config in NVS takes priority over both):

```json
{
  "name": "StickyDeck",
  "pages": [
    {
      "id": "main",
      "title": "Main",
      "cols": 4,
      "rows": 2,
      "buttons": [
        { "label": "Copy",  "action": { "type": "keys",  "mods": ["CTRL"], "keys": ["C"] } },
        { "label": "Paste", "action": { "type": "keys",  "mods": ["CTRL"], "keys": ["V"] } },
        { "label": "Mute",  "action": { "type": "media", "key": "MUTE" } },
        { "label": "Hi",    "action": { "type": "text",  "text": "Hello!" } },
        { "label": "Media", "action": { "type": "page",  "target": "media" } }
      ]
    }
  ]
}
```

Action types: `keys` (modifiers + keycodes), `text`, `media`, `page`, `wifi`
(toggle the config AP on/off).
Modifiers: `CTRL`, `SHIFT`, `ALT`, `GUI`. Keys: single chars, `ENTER`, `SPACE`,
`ESC`, `TAB`, `F1`-`F12`, arrows, `HOME`, `END`, etc.
Media: `VOLUME_UP`, `VOLUME_DOWN`, `MUTE`, `PLAY`, `PAUSE`, `PLAY_PAUSE`,
`NEXT`, `PREV`, `STOP`.

Each button may also set an `"icon"` (drawn above the label):
`annotate`, `arrow`, `circle`, `whiteboard`, `text`, `zoom`.

Set `"wifi": false` at the top level to keep the config AP off at boot
(use a `wifi`-type button to switch it back on).
Set `"beep": true` to enable button/Wi-Fi-toggle beeps (off by default).
Set `"sleep": N` to auto deep-sleep after N idle minutes (`0` = never, default).
Set `"ap_password": "..."` to change the Wi-Fi AP password (default `stickydeck`).

## Build / flash

ESP-IDF v5.4, target `esp32s3`:

```sh
cd /home/roel/pi-cwd-20260930/stickydeck/main
source /home/roel/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor
```

See `docs/setup.md` for the full toolchain/repo setup and `docs/plan.md` for the
original implementation plan.
