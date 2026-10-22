#include "deck_config.h"

#include <cstring>
#include <initializer_list>

#include "ble_hid.h"
#include "cJSON.h"
#include "deck_actions.h"
#include "esp_log.h"
#include "sticky_sdcard.h"

namespace {

constexpr char kTag[] = "deck_config";
constexpr size_t kJsonBufferSize = 4096;

bool parse_action(cJSON *action_json, DeckAction &action)
{
    if (action_json == nullptr) {
        return false;
    }

    const cJSON *type_json = cJSON_GetObjectItem(action_json, "type");
    const char *type = (type_json != nullptr && cJSON_IsString(type_json))
                           ? type_json->valuestring
                           : "";

    if (strcmp(type, "keys") == 0) {
        action.type = DeckActionType::Keys;
        action.modifiers = 0;
        action.keycodes.clear();

        const cJSON *mods = cJSON_GetObjectItem(action_json, "mods");
        if (mods != nullptr && cJSON_IsArray(mods)) {
            cJSON *m = nullptr;
            cJSON_ArrayForEach(m, mods) {
                if (cJSON_IsString(m)) {
                    uint8_t bit = 0;
                    if (deck_modifier_from_name(m->valuestring, &bit)) {
                        action.modifiers |= bit;
                    }
                }
            }
        }

        const cJSON *keys = cJSON_GetObjectItem(action_json, "keys");
        if (keys != nullptr && cJSON_IsArray(keys)) {
            cJSON *k = nullptr;
            cJSON_ArrayForEach(k, keys) {
                if (cJSON_IsString(k)) {
                    uint8_t code = 0;
                    if (deck_keycode_from_name(k->valuestring, &code)) {
                        action.keycodes.push_back(code);
                    }
                }
            }
        }
        return !action.keycodes.empty();
    }

    if (strcmp(type, "text") == 0) {
        const cJSON *text = cJSON_GetObjectItem(action_json, "text");
        if (text != nullptr && cJSON_IsString(text)) {
            action.type = DeckActionType::Text;
            action.text = text->valuestring;
            return true;
        }
        return false;
    }

    if (strcmp(type, "media") == 0) {
        const cJSON *key = cJSON_GetObjectItem(action_json, "key");
        if (key != nullptr && cJSON_IsString(key)) {
            uint16_t usage = 0;
            if (deck_media_from_name(key->valuestring, &usage)) {
                action.type = DeckActionType::Media;
                action.media_usage = usage;
                return true;
            }
        }
        return false;
    }

    if (strcmp(type, "page") == 0) {
        const cJSON *target = cJSON_GetObjectItem(action_json, "target");
        if (target != nullptr && cJSON_IsString(target)) {
            action.type = DeckActionType::Page;
            action.page_target = target->valuestring;
            return true;
        }
        return false;
    }

    return false;
}

bool parse_page(cJSON *page_json, DeckPage &page)
{
    if (page_json == nullptr) {
        return false;
    }

    const cJSON *id_json = cJSON_GetObjectItem(page_json, "id");
    const cJSON *title_json = cJSON_GetObjectItem(page_json, "title");
    if (id_json != nullptr && cJSON_IsString(id_json)) {
        page.id = id_json->valuestring;
    }
    if (title_json != nullptr && cJSON_IsString(title_json)) {
        page.title = title_json->valuestring;
    }

    const cJSON *cols = cJSON_GetObjectItem(page_json, "cols");
    const cJSON *rows = cJSON_GetObjectItem(page_json, "rows");
    if (cols != nullptr && cJSON_IsNumber(cols) && cols->valueint > 0) {
        page.cols = static_cast<uint8_t>(cols->valueint);
    }
    if (rows != nullptr && cJSON_IsNumber(rows) && rows->valueint > 0) {
        page.rows = static_cast<uint8_t>(rows->valueint);
    }

    page.buttons.clear();
    const cJSON *buttons = cJSON_GetObjectItem(page_json, "buttons");
    if (buttons != nullptr && cJSON_IsArray(buttons)) {
        cJSON *button_json = nullptr;
        cJSON_ArrayForEach(button_json, buttons) {
            DeckButton button;
            const cJSON *label = cJSON_GetObjectItem(button_json, "label");
            if (label != nullptr && cJSON_IsString(label)) {
                button.label = label->valuestring;
            }
            if (parse_action(cJSON_GetObjectItem(button_json, "action"),
                             button.action)) {
                page.buttons.push_back(button);
            }
        }
    }
    return !page.buttons.empty();
}

bool parse_json(const char *json, DeckConfig &config)
{
    cJSON *root = cJSON_Parse(json);
    if (root == nullptr) {
        return false;
    }

    const cJSON *name = cJSON_GetObjectItem(root, "name");
    if (name != nullptr && cJSON_IsString(name)) {
        config.name = name->valuestring;
    }

    config.pages.clear();
    const cJSON *pages = cJSON_GetObjectItem(root, "pages");
    if (pages != nullptr && cJSON_IsArray(pages)) {
        cJSON *page_json = nullptr;
        cJSON_ArrayForEach(page_json, pages) {
            DeckPage page;
            if (parse_page(page_json, page)) {
                config.pages.push_back(page);
            }
        }
    }

    cJSON_Delete(root);
    return !config.pages.empty();
}

void add_key_button(std::vector<DeckButton> &buttons, const char *label,
                    uint8_t mods, std::initializer_list<uint8_t> keys)
{
    DeckButton b;
    b.label = label;
    b.action.type = DeckActionType::Keys;
    b.action.modifiers = mods;
    for (uint8_t k : keys) {
        b.action.keycodes.push_back(k);
    }
    buttons.push_back(b);
}

void add_text_button(std::vector<DeckButton> &buttons, const char *label,
                     const char *text)
{
    DeckButton b;
    b.label = label;
    b.action.type = DeckActionType::Text;
    b.action.text = text;
    buttons.push_back(b);
}

void add_media_button(std::vector<DeckButton> &buttons, const char *label,
                      uint16_t usage)
{
    DeckButton b;
    b.label = label;
    b.action.type = DeckActionType::Media;
    b.action.media_usage = usage;
    buttons.push_back(b);
}

void add_page_button(std::vector<DeckButton> &buttons, const char *label,
                     const char *target)
{
    DeckButton b;
    b.label = label;
    b.action.type = DeckActionType::Page;
    b.action.page_target = target;
    buttons.push_back(b);
}

}  // namespace

DeckConfig deck_config_default()
{
    DeckConfig config;
    config.name = "StickyDeck";

    DeckPage main;
    main.id = "main";
    main.title = "Main";
    main.cols = 4;
    main.rows = 2;
    add_key_button(main.buttons, "Copy", 0x01, {0x06});   /* Ctrl+C */
    add_key_button(main.buttons, "Paste", 0x01, {0x19});  /* Ctrl+V */
    add_media_button(main.buttons, "Mute", BLE_HID_MEDIA_MUTE);
    add_media_button(main.buttons, "Vol+", BLE_HID_MEDIA_VOLUME_UP);
    add_text_button(main.buttons, "Hello!", "Hello from StickyDeck! ");
    add_media_button(main.buttons, "Vol-", BLE_HID_MEDIA_VOLUME_DOWN);
    add_page_button(main.buttons, "Media >", "media");
    add_key_button(main.buttons, "Lock", 0x08, {0x0F});   /* GUI+L */
    config.pages.push_back(main);

    DeckPage media;
    media.id = "media";
    media.title = "Media";
    media.cols = 4;
    media.rows = 2;
    add_media_button(media.buttons, "Prev", BLE_HID_MEDIA_PREV);
    add_media_button(media.buttons, "Play", BLE_HID_MEDIA_PLAY_PAUSE);
    add_media_button(media.buttons, "Next", BLE_HID_MEDIA_NEXT);
    add_media_button(media.buttons, "Stop", BLE_HID_MEDIA_STOP);
    add_page_button(media.buttons, "< Main", "main");
    config.pages.push_back(media);

    return config;
}

esp_err_t deck_config_load(DeckConfig &config)
{
    static char json_buffer[kJsonBufferSize] = {};
    json_buffer[0] = '\0';

    const esp_err_t read_result =
        sticky_sdcard_read_text("deck.json", json_buffer, sizeof(json_buffer));

    if (read_result == ESP_OK && json_buffer[0] != '\0' &&
        parse_json(json_buffer, config)) {
        ESP_LOGI(kTag, "Loaded deck.json from microSD: %s",
                 config.name.c_str());
        return ESP_OK;
    }

    ESP_LOGW(kTag, "deck.json not available (%s); using built-in default",
             esp_err_to_name(read_result));
    config = deck_config_default();
    return ESP_OK;
}
