# reTerminal Sticky Dashboard Demo

[中文说明](README-ZH.md)

An ESP-IDF demo for developing with the onboard hardware of the reTerminal
Sticky. It combines the e-paper display, buttons, touch panel, environmental
sensor, RTC, battery gauge, MicroSD card, IMU, microphone, buzzer, and deep
sleep in one working application.

The project can be flashed as a complete dashboard or used as a reference when
building a custom application for the Sticky. Hardware access, application
state, and page rendering are kept separate so individual modules can be reused
without copying the whole UI.

## Features

The dashboard contains six pages:

| Page | Function |
| --- | --- |
| Home | Text, lines, rectangles, and four-level grayscale |
| Sensor | SHT40 temperature and humidity |
| Battery | BQ27220 state of charge and external-power status |
| Note | Text read from `/TEST.TXT` on a MicroSD card |
| IMU | Gravity arrow driven by the LSM6DS3 accelerometer |
| Microphone | One-second PDM sample with RMS, peak, and level bar |

The PCF8563 time is shown in the shared status bar on the Sensor, Battery,
Note, IMU, and Microphone pages. It is refreshed once per minute with a
black-and-white partial update. The Demo also supports button and touch
navigation, buzzer feedback, and deep sleep with page-state restoration.

## Requirements

- reTerminal Sticky
- USB-C cable
- ESP-IDF v5.4
- A FAT-formatted MicroSD card for the Note page (optional)

Project configuration:

- Target: ESP32-S3
- Flash: 32 MB
- PSRAM: 80 MHz Octal PSRAM
- Logical display resolution: 800 x 480

If ESP-IDF is not installed, follow the official
[ESP-IDF Get Started guide](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/get-started/).

## Build and Run

Open an ESP-IDF v5.4 terminal in the project directory, then run:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port of the device, for example `COM5` on
Windows. Press `Ctrl+]` to exit the serial monitor.

If generated build files become inconsistent, remove only the generated
`build/` directory and build again.

### Prepare the MicroSD card

Format the card with a FAT filesystem and create this file in its root:

```text
TEST.TXT
```

Example content:

```text
Hello Sticky! Happy coding!
```

The Demo reads up to 256 characters. Printable ASCII text is recommended
because the bundled bitmap font is designed for ASCII.

## Controls

| Input | Action |
| --- | --- |
| `UP` button | Previous page |
| `DOWN` button | Next page |
| Swipe right | Previous page |
| Swipe left | Next page |
| Short press `AI` | Refresh the current page |
| Long press `AI` for about 2 seconds | Beep and enter deep sleep |
| Press `AI` while sleeping | Wake and restore the previous page |

Touch accepts horizontal swipes only. Taps, short movements, and mostly
vertical gestures are ignored to reduce accidental page changes.

## Project Layout

```text
reTerminal-Sticky-Dashboard-Demo/
|-- components/                 Third-party and panel components
|   |-- bq27220/
|   |-- button/
|   |-- gt911/
|   `-- seeed_epaper/
`-- main/
    |-- main.cpp                Startup and main event loop
    |-- pin_config.h            Sticky pin and I2C address definitions
    |-- board/                  Shared power, GPIO, and sensor I2C resources
    |-- devices/                Hardware-facing device modules
    |-- app/                    State, data updates, events, and navigation
    |-- pages/                  Page rendering functions
    `-- ui/                     Canvas, font, and shared status bar
```

The main data flow is:

```text
Hardware -> devices -> app_data -> AppState -> pages -> Canvas -> display
```

- `board/` owns resources shared by multiple devices.
- `devices/` initializes hardware and returns readings.
- `app/app_data.cpp` copies device readings into `AppState`.
- `pages/` reads `AppState` and draws without accessing hardware.
- `app/app.cpp` handles events, navigation, rendering, and display refresh.

`main.cpp` shows the required initialization order and is the best starting
point for understanding the complete application.

## Device APIs

Each onboard peripheral has a small API under `main/devices/`:

| Module | Main API | Purpose |
| --- | --- | --- |
| `sticky_display` | `init`, `canvas`, `refresh`, `refresh_partial`, `sleep` | E-paper framebuffer and refresh |
| `sticky_buttons` | `init` | UP, DOWN, and AI button events |
| `sticky_touch` | `init`, `stop` | GT911 swipe events |
| `sticky_buzzer` | `init`, `beep` | Short operation feedback |
| `sticky_sht40` | `init`, `read` | Temperature and humidity |
| `sticky_rtc` | `init`, `read` | Read-only date and time |
| `sticky_battery` | `init`, `read` | Battery state of charge |
| `sticky_charger` | `init`, `read` | External-power status |
| `sticky_sdcard` | `init`, `read_text` | Read a text file from MicroSD |
| `sticky_imu` | `init`, `start_monitoring`, `get_state` | Stable device orientation |
| `sticky_microphone` | `init`, `capture` | One-shot PDM audio measurement |
| `sticky_power` | `enter_deep_sleep` | Deep-sleep entry and AI wake-up |

Refer to the matching `.h` file for the exact types and return values.
Runtime functions return `esp_err_t`; callers should check the result before
using a reading.

### Reading an I2C sensor

The battery gauge, RTC, SHT40, and IMU use the shared sensor bus created by the
board layer. Pass that existing bus handle to the device initializer:

```cpp
ESP_ERROR_CHECK(board_init());
ESP_ERROR_CHECK(sticky_sht40_init(board_sensor_i2c_bus()));

Sht40Reading reading = {};
if (sticky_sht40_read(reading) == ESP_OK) {
    printf("Temperature: %.1f C\n", reading.temperature_c);
    printf("Humidity: %.1f %%\n", reading.humidity_percent);
}
```

Do not create another I2C bus on GPIO0/GPIO1 for an individual sensor.

### Drawing on the e-paper display

The display module owns the framebuffer and exposes it through `Canvas`:

```cpp
ESP_ERROR_CHECK(sticky_display_init());

Canvas *canvas = sticky_display_canvas();
canvas->clear(GrayLevel::White);
canvas->draw_text(60, 80, "Hello Sticky", 3, GrayLevel::Black);
canvas->draw_line(60, 130, 740, 130, GrayLevel::DarkGray);
canvas->draw_rect(60, 170, 260, 120, GrayLevel::Black);

ESP_ERROR_CHECK(sticky_display_refresh());
```

All UI code uses an 800 x 480 top-left-origin coordinate system. The required
180-degree physical panel rotation is already handled by `sticky_display`.

The available canvas colors are:

```cpp
GrayLevel::Black
GrayLevel::DarkGray
GrayLevel::LightGray
GrayLevel::White
```

One logical Canvas area can be updated without refreshing the complete screen:

```cpp
// Change only the intended black-and-white Canvas pixels first.
ESP_ERROR_CHECK(sticky_display_refresh_partial());
```

The partial-refresh API performs the physical 180-degree conversion internally.
It uses the panel's black-and-white partial waveform, so the changed area should
contain only `Black` and `White` pixels. The Dashboard uses this API only for
the RTC time on its black-and-white pages.

### Rendering application data

The complete Demo does not let pages read hardware directly. A device reading
is copied into `AppState`, then rendered by a page:

```text
sticky_sht40_read()
-> update_environment(state)
-> state.environment
-> sensor_page_render(canvas, state)
-> sticky_display_refresh()
```

This separation keeps pages predictable and makes hardware modules reusable in
applications with a different UI.

To add another page, add a `PageId`, create a `pages/*_page.cpp/.h` renderer,
connect it in `app/app.cpp`, and add its source file to `main/CMakeLists.txt`.
Use `app_data` when the page needs hardware data.

## Important Hardware Notes

### Shared SPI bus

The e-paper display and MicroSD card share SPI2 signal lines and use different
chip-select pins. The display initializes SPI2 first. SD access temporarily
mounts the card on the existing bus, reads and closes the file, then unmounts
the card before the display is refreshed.

The SD module must not free SPI2 because the display remains attached to it.
When changing the SPI bus configuration, keep every unused data pin set to
`-1`; a zero-initialized unused pin can claim GPIO0 and break the sensor I2C
clock.

### I2C buses

- I2C1 on GPIO0/GPIO1: BQ27220, PCF8563, SHT40, and LSM6DS3.
- I2C0 on GPIO2/GPIO3: GT911 touch controller.

Keep the GT911 on its dedicated bus unless its reset, address selection, and
coordinate conversion are also reviewed.

### Display refresh

The Home page demonstrates a four-level grayscale full refresh:

```cpp
sticky_display_refresh();
```

The Sensor, Battery, Note, IMU, Microphone, and deep-sleep pages use a
black-and-white full refresh:

```cpp
sticky_display_refresh_monochrome();
```

The RTC time area on the black-and-white pages is updated once per minute with
`sticky_display_refresh_partial()`. The Home page does not display the status
bar time. Other page content is not refreshed in the background. Page changes
and AI-button refreshes use the full-refresh mode assigned to the current page.

`app/app_clock.cpp` aligns the first timer expiration to the next RTC minute
and posts `AppEvent::ClockMinuteTick`. The callback never accesses the RTC or
display directly; the normal app event loop performs the read and partial
refresh.

Repeated partial updates can gradually leave light ghosting on an e-paper
panel. Short-press AI to perform a normal full refresh when needed. Brief
flashing or old-image visibility during a full refresh is normal for e-paper.

### MicroSD reload behavior

The Note page reads `TEST.TXT` whenever the page is entered or refreshed with
the AI button. The filesystem is mounted only for the read operation, so the
card can be removed or its file can be changed between reads.

### Deep sleep

Before sleeping, the application stores `AppState` in RTC memory, stops touch
polling, renders a monochrome sleep page, powers down the panel, and configures
GPIO4 as the wake source. Wake-up restarts the ESP32-S3 and restores the page
that was previously visible.

## Pin Definitions

All verified pin assignments and I2C addresses are defined in
[`main/pin_config.h`](main/pin_config.h). Use that file as the source of truth
instead of duplicating pin numbers in application code.

Key shared connections:

| Function | Pins |
| --- | --- |
| Sensor I2C1 | SCL GPIO0, SDA GPIO1 |
| Touch I2C0 | SCL GPIO2, SDA GPIO3 |
| UP / DOWN / AI | GPIO5 / GPIO6 / GPIO4 |
| E-paper SPI | MOSI GPIO14, MISO GPIO12, CLK GPIO13, CS GPIO15 |
| MicroSD | CS GPIO8, detect GPIO11, enable GPIO10; shares e-paper SPI |
| PDM microphone | CLK GPIO19, data GPIO20, enable GPIO38 |

## Troubleshooting

- **Sensor I2C transactions fail:** confirm that SPI unused pins remain `-1`
  and that no second bus was created on GPIO0/GPIO1.
- **Touch direction is reversed:** do not rotate coordinates in page code;
  keep the verified conversion in `sticky_touch`.
- **MicroSD changes are not visible:** enter the Note page again or short-press
  AI to reload `TEST.TXT`.
- **The display flashes while changing pages:** this is expected during the
  current full-screen e-paper refresh.
- **The build directory reports missing generated files:** remove only
  `build/` and run `idf.py build` again.

## Extending the Demo

Keep the existing layer boundaries when adding hardware or UI features:

- Put pin definitions in `pin_config.h`.
- Put shared buses and board power sequencing in `board/`.
- Put hardware access in `devices/`.
- Convert readings to displayable state in `app_data`.
- Let pages read state and draw only through `Canvas`.
- Route input and asynchronous actions through `AppEvent`.
- Keep orientation conversion inside the display and touch modules.

Following these rules allows a new feature to be added without changing the
behavior of the existing pages.
