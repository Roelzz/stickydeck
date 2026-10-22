#include "deck_actions.h"

#include <cctype>
#include <cstring>

#include "ble_hid.h"
#include "deck_config.h"

namespace {

struct KeyName {
    const char *name;
    uint8_t code;
};

/* HID keyboard usage IDs (US layout). */
constexpr KeyName kKeyNames[] = {
    {"ENTER", 0x28}, {"RETURN", 0x28}, {"ESC", 0x29}, {"ESCAPE", 0x29},
    {"BACKSPACE", 0x2A}, {"TAB", 0x2B}, {"SPACE", 0x2C},
    {"MINUS", 0x2D}, {"EQUAL", 0x2E}, {"LEFTBRACKET", 0x2F},
    {"RIGHTBRACKET", 0x30}, {"BACKSLASH", 0x31}, {"SEMICOLON", 0x33},
    {"QUOTE", 0x34}, {"GRAVE", 0x35}, {"COMMA", 0x36}, {"PERIOD", 0x37},
    {"SLASH", 0x38}, {"CAPSLOCK", 0x39},
    {"F1", 0x3A}, {"F2", 0x3B}, {"F3", 0x3C}, {"F4", 0x3D}, {"F5", 0x3E},
    {"F6", 0x3F}, {"F7", 0x40}, {"F8", 0x41}, {"F9", 0x42}, {"F10", 0x43},
    {"F11", 0x44}, {"F12", 0x45},
    {"PRINTSCREEN", 0x46}, {"SCROLLLOCK", 0x47}, {"PAUSE", 0x48},
    {"INSERT", 0x49}, {"HOME", 0x4A}, {"PAGEUP", 0x4B}, {"DELETE", 0x4C},
    {"END", 0x4D}, {"PAGEDOWN", 0x4E},
    {"RIGHT", 0x4F}, {"LEFT", 0x50}, {"DOWN", 0x51}, {"UP", 0x52},
};

struct MediaName {
    const char *name;
    uint16_t usage;
};

constexpr MediaName kMediaNames[] = {
    {"VOLUME_UP", BLE_HID_MEDIA_VOLUME_UP},
    {"VOLUME_DOWN", BLE_HID_MEDIA_VOLUME_DOWN},
    {"MUTE", BLE_HID_MEDIA_MUTE},
    {"PLAY", BLE_HID_MEDIA_PLAY},
    {"PAUSE", BLE_HID_MEDIA_PAUSE},
    {"PLAY_PAUSE", BLE_HID_MEDIA_PLAY_PAUSE},
    {"NEXT", BLE_HID_MEDIA_NEXT},
    {"PREV", BLE_HID_MEDIA_PREV},
    {"STOP", BLE_HID_MEDIA_STOP},
};

}  // namespace

bool deck_keycode_from_name(const char *name, uint8_t *out)
{
    if (name == nullptr || out == nullptr) {
        return false;
    }

    /* Single character: letters, digits, and common symbols. */
    if (name[0] != '\0' && name[1] == '\0') {
        const char c = name[0];
        if (c >= 'a' && c <= 'z') {
            *out = 0x04 + static_cast<uint8_t>(c - 'a');
            return true;
        }
        if (c >= 'A' && c <= 'Z') {
            *out = 0x04 + static_cast<uint8_t>(c - 'A');
            return true;
        }
        if (c >= '1' && c <= '9') {
            *out = 0x1E + static_cast<uint8_t>(c - '1');
            return true;
        }
        if (c == '0') {
            *out = 0x27;
            return true;
        }
    }

    for (const auto &entry : kKeyNames) {
        if (strcmp(name, entry.name) == 0) {
            *out = entry.code;
            return true;
        }
    }
    return false;
}

bool deck_modifier_from_name(const char *name, uint8_t *out)
{
    if (name == nullptr || out == nullptr) {
        return false;
    }
    if (strcmp(name, "CTRL") == 0 || strcmp(name, "CONTROL") == 0) {
        *out = BLE_HID_MOD_CTRL;
        return true;
    }
    if (strcmp(name, "SHIFT") == 0) {
        *out = BLE_HID_MOD_SHIFT;
        return true;
    }
    if (strcmp(name, "ALT") == 0 || strcmp(name, "OPTION") == 0) {
        *out = BLE_HID_MOD_ALT;
        return true;
    }
    if (strcmp(name, "GUI") == 0 || strcmp(name, "WIN") == 0 ||
        strcmp(name, "CMD") == 0 || strcmp(name, "META") == 0) {
        *out = BLE_HID_MOD_GUI;
        return true;
    }
    return false;
}

bool deck_media_from_name(const char *name, uint16_t *out)
{
    if (name == nullptr || out == nullptr) {
        return false;
    }
    for (const auto &entry : kMediaNames) {
        if (strcmp(name, entry.name) == 0) {
            *out = entry.usage;
            return true;
        }
    }
    return false;
}

esp_err_t deck_action_execute(const DeckAction &action)
{
    switch (action.type) {
    case DeckActionType::Keys:
        if (action.keycodes.empty()) {
            return ESP_ERR_INVALID_ARG;
        }
        return ble_hid_tap_keys(action.modifiers, action.keycodes.data(),
                                action.keycodes.size());
    case DeckActionType::Text:
        return ble_hid_type_text(action.text.c_str());
    case DeckActionType::Media:
        return ble_hid_tap_media(action.media_usage);
    case DeckActionType::Page:
    case DeckActionType::Wifi:
        /* Handled by the deck app, not the HID layer. */
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}
