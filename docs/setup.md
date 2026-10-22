# Setup & Build Notes

## Hardware

- reTerminal Sticky = ESP32-S3R8 (8 MB PSRAM, 32 MB flash), 3.97" ePaper (SSD1677),
  GT911 capacitive touch, BLE 5.0.
- USB-C is **UART only** (CH343 bridge → `/dev/ttyACM0`).
- Serial by-id: `usb-1a86_USB_Single_Serial_5C84335532`
- Chip MAC: `b8:1f:3f:d8:b4:b4`, revision v0.2.

## Toolchain

- ESP-IDF **v5.4** installed at `/home/roel/esp/esp-idf`.
- Python env: `/home/roel/.espressif/python_env/idf5.4_py3.14_env`.
- Activate per-shell:
  ```sh
  source /home/roel/esp/esp-idf/export.sh
  ```

## Build / flash / monitor

```sh
cd /home/roel/pi-cwd-20260930/stickydeck/main
source /home/roel/esp/esp-idf/export.sh
idf.py set-target esp32s3      # first time only
idf.py build
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor   # Ctrl+] to exit
```

## Flash backup

Original factory firmware backed up before flashing:
`/home/roel/pi-cwd-20260930/backups/sticky-flash-backup-20261022-121646.bin`
(32 MB, md5 `d328df362a98521856b42ce445a12da8`).

Restore if needed:
```sh
esptool --port /dev/ttyACM0 write_flash 0x0 sticky-flash-backup-20261022-121646.bin
```

## Repo / worktrees

Bare hub: `/home/roel/pi-cwd-20260930/stickydeck/stickydeck.git`

| Worktree | Branch |
|---|---|
| `stickydeck/main` | `main` |
| `stickydeck/worktrees/ble-hid` | `feature/ble-hid` |
| `stickydeck/worktrees/touch-driver` | `feature/touch-driver` |
| `stickydeck/worktrees/render-ui` | `feature/render-ui` |
| `stickydeck/worktrees/action-engine` | `feature/action-engine` |
| `stickydeck/worktrees/config` | `feature/config` |

Reference demos: `/home/roel/pi-cwd-20260930/reference/`
