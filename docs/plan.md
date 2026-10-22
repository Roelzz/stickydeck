# Plan: Turn the reTerminal Sticky into a FreeTouchDeck-style BLE Macro Deck

**Goal:** Use (or build) firmware so the Seeed reTerminal Sticky acts like
[FreeTouchDeck](https://github.com/DustinWatts/FreeTouchDeck) — a touchscreen
keypad that emulates a BLE keyboard and fires macros, shortcuts, text, media
keys, and app launches on a paired computer.

---

## 1. Summary of findings

### 1.1 The target hardware (reTerminal Sticky)

From Seeed's official docs (hardware overview + ESP-IDF guides):

| Subsystem | Detail | Pins |
|---|---|---|
| MCU | ESP32-S3R8, dual-core LX7 @ 240 MHz, 8 MB PSRAM | — |
| Flash | 32 MB QSPI | — |
| Display | 3.97" B&W ePaper, 800×480, 4-level gray, **SSD1677 controller** | SPI2: SCK 13, SDI 14, CS 15, DC 16, RST 17, BUSY 18, EN 47 |
| Touch | **GT911** capacitive, 5-point | I²C0: SCL 2, SDA 3, INT 21, RST 41, EN 42 |
| Buttons | AI/Power, Up, Down | 4, 5, 6 |
| Buzzer | PWM | 48 |
| microSD | SPI (shares SPI2 bus with ePaper) | CS 8, MISO 12, SCK 13, MOSI 14 |
| Sensors | SHT40, RTC (PCF8563), IMU (LSM6DS3TR-C), fuel gauge (BQ27220) | I²C1: GPIO 0/1 |
| Mic | PDM | 19/20 |
| Wireless | Wi-Fi 2.4 GHz + **BLE 5.0** | — |
| USB | USB-C — **UART only** (TX 43 / RX 44) | no native USB-HID |
| Battery | 750 mAh, ~7 days standby | — |

### 1.2 The reference project (FreeTouchDeck)

- ESP32 + TFT touchscreen acting as a **BLE HID keyboard**.
- **Web configurator** (ESPAsyncWebServer + ArduinoJson) edits JSON stored in SPIFFS.
- **Action engine**: key combos, typed text, media keys, app launch, URL open,
  multi-page "menus", sleep/brightness control.
- Optional desktop **Helper app** (app launch, run scripts, auto page-switch on
  app focus).
- Libraries: TFT_eSPI, Adafruit GFX, ESP32-BLE-Keyboard, AsyncTCP/ESPAsyncWebServer,
  ArduinoJson, plus a touch driver (XPT2046 or FT6236).

### 1.3 The critical design constraint: ePaper ≠ TFT

- ePaper **full refresh** is slow (~1–2 s, with visible flash); **partial refresh**
  is faster (~100–300 ms) but **monochrome-only**.
- ePaper **retains the image at zero power** — ideal for a persistent deck.
- FreeTouchDeck's "tap → instant visual highlight → keystroke" loop does **not**
  translate directly. The correct loop for Sticky is:

  ```
  touch-down → fire BLE action IMMEDIATELY → buzzer beep (instant feedback)
            → partial-refresh a small "last key" highlight (async, best-effort)
  ```

- Because the ePaper never needs power to keep showing the button grid, the deck
  can sleep and still *look* live — a nice win over a TFT deck.

---

## 2. Recommended approach (decision)

**Build a new, custom ESP-IDF firmware ("StickyDeck")** on top of Seeed's
official ESP-IDF framework — **do not try to port FreeTouchDeck directly.**

Reasons:

1. FreeTouchDeck is tightly coupled to TFT_eSPI / Adafruit GFX / Arduino and to
   resistive (XPT2046) or FT6236 touch. The **display and touch layers would be a
   near-total rewrite anyway**, so there is little to reuse from the C++ sources.
2. Seeed ships a clean, documented **ESP-IDF v5.4** framework
   (`Board / Device / Page / Canvas / Display` + `AppState` + `AppEvent`) that
   already handles the hard parts: ePaper full/partial refresh modes, deep sleep,
   touch, buttons, buzzer, and the tricky SPI/I²C bus sharing.
3. Native ESP-IDF gives the best control over **partial refresh** and **BLE HID**.

**What we *do* reuse from FreeTouchDeck** (to keep the same UX and save effort):
- Its **JSON config schema** (buttons with symbol/label/action, menus, sleep).
- Its **web editor HTML/JS** (configurator page).
- Its **helper desktop app** design (app launch, script run, auto page switch).
- Its **action vocabulary** (key combos, text, media, launch, URL, page nav).

### Alternative route (only if you specifically want the Arduino/FreeTouchDeck codebase)

Use the **Arduino or PlatformIO starter** in Seeed's
[playground registry](https://github.com/Seeed-Projects/reterminal-sticky-playground-registry)
and re-implement FreeTouchDeck by swapping:
- display: TFT_eSPI → **GxEPD2 (SSD1677)** or Seeed's own ePaper driver
- touch: XPT2046/FT6236 → **GT911**
- refresh strategy: continuous → partial/full on demand

This is more code churn than the ESP-IDF route and fights the hardware rather
than using Seeed's tuned drivers. Not recommended.

---

## 3. Architecture

```
                     ┌─────────────────────────────────────────┐
  microSD / flash     │  Config (JSON): pages, buttons, actions │
  ────────────────►   └─────────────────────────────────────────┘
                              │ parse / hot-reload
                              ▼
  GT911 touch ──► Touch driver (raw tap coordinates) ─┐
  Up/Down/AI  ──► Button driver ──────────────────────┤
                                                       ▼
                              ┌──────────────────────────────────────┐
                              │         App event loop               │
                              │  hit-test grid → Action engine        │
                              │  page navigation, sleep, refresh      │
                              └───────┬───────────────────┬──────────┘
                                      │                   │
                           ┌──────────▼─────────┐   ┌─────▼──────────────┐
                           │  ePaper renderer    │   │  BLE HID keyboard  │
                           │  (grid, pages,      │   │  (keyboard report + │
                           │   partial refresh)  │   │   consumer/media)   │
                           └────────────────────┘   └────────────────────┘
                                                          │ BLE
                                                     ┌────▼─────┐
                                                     │ Host PC  │
                                                     │ (Win/mac/│
                                                     │  Linux)  │
                                                     └──────────┘
```

### 3.1 Modules to build

| Module | Based on | Notes |
|---|---|---|
| `sticky_touch_tap` | extend Seeed `sticky_touch` | **Critical**: Seeed's demo only detects *swipes*. We need raw GT911 tap coordinates for hit-testing. Either extend their driver or write a minimal GT911 driver for 1-point tap + release events. |
| `deck_config` | JSON (cJSON/jsmn) | Parse the button/page/action schema. Load from microSD and/or flash; validate; hot-reload. |
| `deck_render` | Seeed `Canvas` + `sticky_display` | Draw button grid (icons + labels) in monochrome; full refresh on page change; partial refresh for "last key" highlight. |
| `deck_actions` | FreeTouchDeck action model | Execute key combos, text, media keys, page nav, delay/repeat, app-launch/URL. |
| `ble_hid` | ESP-IDF `esp_hid` / HID device example | BLE HID keyboard + consumer (media) reports. |
| `web_config` (phase 5) | FreeTouchDeck editor UI | Wi-Fi AP + HTTP server to edit config in a browser. |
| `host_helper` (phase 6) | FreeTouchDeck-Helper concept | Desktop app for app launch / scripts / auto page-switch. |

### 3.2 Action vocabulary (mirrors FreeTouchDeck)

1. **Keystroke / hotkey** — modifiers + keycode(s) (e.g., `Ctrl+Shift+P`, `Win+L`).
2. **Type text** — send a string of HID keycodes.
3. **Media / consumer keys** — volume, mute, play/pause, etc.
4. **Page navigation** — switch deck menu.
5. **App launch / URL** — via a keystroke sequence (e.g., `Win+R` + command) or the
   desktop helper.
6. **Special** — sleep deck, brightness (buzzer volume), etc.

### 3.3 Config format (adapting FreeTouchDeck's JSON)

```json
{
  "general": { "name": "StickyDeck", "sleepTimeout": 60, "beep": true },
  "pages": [
    {
      "id": "main",
      "title": "Main",
      "grid": { "cols": 4, "rows": 2 },
      "buttons": [
        { "label": "Copy", "action": { "type": "keys", "mods": ["CTRL"], "keys": ["C"] } },
        { "label": "Paste", "action": { "type": "keys", "mods": ["CTRL"], "keys": ["V"] } },
        { "label": "Mute", "action": { "type": "media", "key": "MUTE" } },
        { "label": "Terminal", "action": { "type": "launch", "command": "cmd" } },
        { "label": "Page 2", "action": { "type": "page", "target": "obs" } }
      ]
    }
  ]
}
```

---

## 4. Phased implementation plan

### Phase 0 — Toolchain & de-risking (0.5–1 day)
- [ ] Install **ESP-IDF v5.4** (ESP32-S3 target).
- [ ] Build + flash Seeed's **`Sticky_dashboard_demo`**; confirm display, touch,
  buttons, buzzer, RTC, battery all work. (Validates toolchain + hardware.)
- [ ] Build + flash an ESP-IDF **BLE HID keyboard example**; confirm the Sticky
  pairs with Windows/macOS/Linux and types. **This de-risks the core feature
  before any UI work.**

### Phase 1 — Input bring-up for a macro deck (1–2 days)
- [ ] Extend/write GT911 driver to emit **tap-down / tap-up with (x, y)** (and
  keep swipe detection for page nav).
- [ ] Map taps → grid cells (hit-testing) at 800×480.
- [ ] Wire Up/Down buttons = prev/next page; AI = refresh / deep-sleep.
- [ ] Buzzer beep on successful hit.
- [ ] Render a static button grid and verify tap→cell mapping via serial log.

### Phase 2 — Action engine + BLE HID (2–3 days)
- [ ] BLE HID keyboard + consumer (media) reports.
- [ ] Implement action types: keys, text, media, page-nav, delay/repeat.
- [ ] Fire action **on touch-down**; buzzer immediately; **partial refresh** of the
  tapped button (invert/outline) async; full refresh only on page change.

### Phase 3 — Config & persistence (1–2 days)
- [ ] JSON parser + schema + validation + sensible defaults.
- [ ] Load from **microSD** first (easy editing), fallback to flash partition.
- [ ] Hot-reload (e.g., long-press AI, or auto-detect SD file change).

### Phase 4 — UI renderer (2–3 days)
- [ ] Grid layout engine: N×M buttons, icon + label, monochrome (avoid 4-gray for
  buttons to keep partial refresh clean).
- [ ] Multi-page navigation + page indicator.
- [ ] Status bar (battery %, time from RTC, BLE connected icon) using partial
      refresh — reuse Seeed's minute-tick pattern.
- [ ] Periodic **full refresh** to clear partial-refresh ghosting.
- [ ] Deep-sleep: keep last grid on screen at zero power; wake on AI button;
      restore page state (reuse Seeed's RTC-retention pattern).

### Phase 5 — Web configurator (optional, 2–4 days)
- [ ] Wi-Fi AP mode + tiny HTTP server; serve FreeTouchDeck-style editor.
- [ ] Edit/validate/save JSON; reboot to apply.
- [ ] Alternative: skip web server, keep microSD editing (simpler v1).

### Phase 6 — Host helper (optional, 2–4 days)
- [ ] Desktop app (Electron/Python/Tauri) that pairs over BLE/network to run
      arbitrary commands, launch apps, run scripts, and auto-switch deck page on
      app focus (mirror FreeTouchDeck-Helper).

### Phase 7 — Packaging & distribution (1 day)
- [ ] Publish to the **Sticky Playground registry** (browser-flash) following
      `docs/contributing-firmware.md`; include default configs, README, preview.
- [ ] Note: no existing macro-deck firmware was found in the registry, so this is
      a greenfield contribution.

---

## 5. Risks & mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| ePaper latency kills "instant keypad" feel | High | Fire action on touch-down; feedback via buzzer + partial refresh; never block on full refresh. |
| Partial-refresh ghosting | Med | Monochrome-only pages; periodic full refresh; AI button = manual full refresh. |
| GT911 in Seeed demo only reports swipes | High | Write/extend GT911 driver for raw tap coordinates (Phase 1). |
| USB is UART-only → no USB-HID | Med | BLE HID is the only HID path; document clearly; test pairing on all 3 OSes. |
| microSD shares SPI2 with ePaper | Med | Follow Seeed pattern: finish SD read + unmount **before** display refresh. |
| BLE pairing quirks per-OS | Med | Use standard HID profile; test Win/macOS/Linux; provide reconnect-on-press. |
| Battery in active BLE use | Low | 750 mAh is fine for a deck; keep deep-sleep "always-on" display to stretch life. |

---

## 6. First actionable steps (this week)

1. Install ESP-IDF v5.4; build/flash `Sticky_dashboard_demo` — confirm hardware.
2. Flash a BLE HID keyboard example — confirm the Sticky types over BLE.
3. Write the GT911 tap-coordinate driver; log tap (x,y) to serial.
4. Draw a hardcoded 4×2 grid; verify tap→cell hit-testing + buzzer.
5. Then wire the action engine + JSON config.

**Deliverable of v1:** a Sticky that shows a configurable button grid, pairs as a
BLE keyboard, and fires key combos / text / media keys on tap, with buzzer +
partial-refresh feedback and deep-sleep persistence.
