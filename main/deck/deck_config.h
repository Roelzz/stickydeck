#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "esp_err.h"

enum class DeckActionType {
    None,
    Keys,   // hotkey combination
    Text,   // type a string
    Media,  // consumer-control key
    Page,   // switch to another page
    Wifi,   // toggle the Wi-Fi config AP on/off
};

struct DeckAction {
    DeckActionType type = DeckActionType::None;
    uint8_t modifiers = 0;              // Keys: OR of CTRL/SHIFT/ALT/GUI
    std::vector<uint8_t> keycodes;      // Keys: 1..6 HID keycodes
    std::string text;                   // Text
    uint16_t media_usage = 0;           // Media: BLE_HID_MEDIA_* usage
    std::string page_target;            // Page: target page id
};

struct DeckButton {
    std::string label;
    std::string icon;   // optional built-in icon name
    DeckAction action;
};

struct DeckPage {
    std::string id;
    std::string title;
    uint8_t cols = 4;
    uint8_t rows = 2;
    std::vector<DeckButton> buttons;
};

struct DeckConfig {
    std::string name;
    bool wifi = true; /* start the config AP at boot */
    std::vector<DeckPage> pages;
};

/**
 * Load the deck configuration, preferring (in order) NVS (web-saved), the
 * microSD "deck.json", then the embedded default. Always leaves `config` in a
 * usable state on ESP_OK.
 */
esp_err_t deck_config_load(DeckConfig &config);

/** The JSON text currently in effect (NVS > microSD > embedded default). */
esp_err_t deck_config_json(char *buffer, size_t buffer_size);

/** Embedded default configuration, as JSON. */
const char *deck_config_default_json();

/** Persist a JSON configuration to NVS (used by the web editor). */
esp_err_t deck_config_save_nvs(const char *json);
