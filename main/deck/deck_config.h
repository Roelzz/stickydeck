#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "esp_err.h"

enum class DeckActionType {
    None,
    Keys,   // hotkey combination
    Text,   // type a string
    Media,  // consumer-control key
    Page,   // switch to another page
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
    std::vector<DeckPage> pages;
};

/**
 * Load the deck configuration. Tries "deck.json" on the microSD card first;
 * if the card or file is unavailable, falls back to a built-in default.
 * Always leaves `config` in a usable state on ESP_OK.
 */
esp_err_t deck_config_load(DeckConfig &config);

/** A built-in default deck (used when no SD config is present). */
DeckConfig deck_config_default();
