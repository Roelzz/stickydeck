#pragma once

#include <cstdint>

#include "esp_err.h"

struct DeckAction;

/** Execute one action via BLE HID. */
esp_err_t deck_action_execute(const DeckAction &action);

/* Name -> code lookups, shared with config parsing. */
bool deck_keycode_from_name(const char *name, uint8_t *out);
bool deck_modifier_from_name(const char *name, uint8_t *out);
bool deck_media_from_name(const char *name, uint16_t *out);
