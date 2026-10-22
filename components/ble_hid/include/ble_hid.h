#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Keyboard HID modifier bits. */
enum {
    BLE_HID_MOD_CTRL  = 0x01,
    BLE_HID_MOD_SHIFT = 0x02,
    BLE_HID_MOD_ALT   = 0x04,
    BLE_HID_MOD_GUI   = 0x08,
};

/* Consumer-control (media) usage IDs from the USB HID Usage Tables. */
enum {
    BLE_HID_MEDIA_PLAY        = 0xB0,
    BLE_HID_MEDIA_PAUSE       = 0xB1,
    BLE_HID_MEDIA_NEXT        = 0xB5,
    BLE_HID_MEDIA_PREV        = 0xB6,
    BLE_HID_MEDIA_STOP        = 0xB7,
    BLE_HID_MEDIA_PLAY_PAUSE  = 0xCD,
    BLE_HID_MEDIA_MUTE        = 0xE2,
    BLE_HID_MEDIA_VOLUME_UP   = 0xE9,
    BLE_HID_MEDIA_VOLUME_DOWN = 0xEA,
};

/**
 * Initialize the Bluetooth controller, Bluedroid host, and register a BLE HID
 * keyboard + consumer-control (media) device. Advertising starts automatically
 * once the HID device is ready.
 */
esp_err_t ble_hid_init(void);

/** True while a host is connected. */
bool ble_hid_connected(void);

/** The BLE device name this device advertises as. */
const char *ble_hid_device_name(void);

/** Send a raw HID keyboard report (8 bytes: [modifier, 0, key0..key5]). */
esp_err_t ble_hid_send_keyboard_report(const uint8_t report[8]);

/** Press+release a key combination: `modifiers` (OR-ed) plus 1..6 keycodes. */
esp_err_t ble_hid_tap_keys(uint8_t modifiers, const uint8_t *keycodes, size_t count);

/** Press+release a single key with optional modifiers. */
esp_err_t ble_hid_tap_key(uint8_t modifiers, uint8_t keycode);

/** Type a NUL-terminated string using the US keyboard layout. */
esp_err_t ble_hid_type_text(const char *text);

/** Press+release a consumer-control (media) key. */
esp_err_t ble_hid_tap_media(uint16_t usage);

#ifdef __cplusplus
}
#endif
