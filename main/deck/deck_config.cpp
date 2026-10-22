#include "deck_config.h"

#include <cstring>

#include "ble_hid.h"
#include "cJSON.h"
#include "deck_actions.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sticky_sdcard.h"

namespace {

constexpr char kTag[] = "deck_config";
constexpr size_t kJsonBufferSize = 4096;
constexpr char kNvsNamespace[] = "deck";
constexpr char kNvsKey[] = "json";

/* Embedded default configuration (also shown in the web editor). */
const char kDefaultJson[] = R"JSON({
  "name": "StickyDeck",
  "pages": [
    {
      "id": "main",
      "title": "Main",
      "cols": 4,
      "rows": 2,
      "buttons": [
        { "label": "Copy",   "action": { "type": "keys",  "mods": ["CTRL"], "keys": ["C"] } },
        { "label": "Paste",  "action": { "type": "keys",  "mods": ["CTRL"], "keys": ["V"] } },
        { "label": "Mute",   "action": { "type": "media", "key": "MUTE" } },
        { "label": "Vol+",   "action": { "type": "media", "key": "VOLUME_UP" } },
        { "label": "Hello!", "action": { "type": "text",  "text": "Hello from StickyDeck! " } },
        { "label": "Vol-",   "action": { "type": "media", "key": "VOLUME_DOWN" } },
        { "label": "Media >","action": { "type": "page",  "target": "media" } },
        { "label": "Lock",   "action": { "type": "keys",  "mods": ["GUI"], "keys": ["L"] } }
      ]
    },
    {
      "id": "media",
      "title": "Media",
      "cols": 4,
      "rows": 2,
      "buttons": [
        { "label": "Prev",   "action": { "type": "media", "key": "PREV" } },
        { "label": "Play",   "action": { "type": "media", "key": "PLAY_PAUSE" } },
        { "label": "Next",   "action": { "type": "media", "key": "NEXT" } },
        { "label": "Stop",   "action": { "type": "media", "key": "STOP" } },
        { "label": "< Main", "action": { "type": "page",  "target": "main" } }
      ]
    }
  ]
})JSON";

esp_err_t open_nvs(nvs_handle_t *handle)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }
    return nvs_open(kNvsNamespace, NVS_READWRITE, handle);
}

esp_err_t load_nvs(char *buffer, size_t buffer_size)
{
    nvs_handle_t handle = 0;
    esp_err_t err = open_nvs(&handle);
    if (err != ESP_OK) {
        return err;
    }
    size_t length = buffer_size;
    err = nvs_get_blob(handle, kNvsKey, buffer, &length);
    nvs_close(handle);
    if (err == ESP_OK) {
        buffer[length < buffer_size ? length : buffer_size - 1] = '\0';
    }
    return err;
}

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

}  // namespace

const char *deck_config_default_json()
{
    return kDefaultJson;
}

esp_err_t deck_config_save_nvs(const char *json)
{
    if (json == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = open_nvs(&handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, kNvsKey, json, strlen(json) + 1);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t deck_config_json(char *buffer, size_t buffer_size)
{
    if (buffer == nullptr || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 1. NVS (web-saved). */
    if (load_nvs(buffer, buffer_size) == ESP_OK && buffer[0] != '\0') {
        ESP_LOGI(kTag, "Using config from NVS");
        return ESP_OK;
    }

    /* 2. microSD deck.json. */
    if (sticky_sdcard_read_text("deck.json", buffer, buffer_size) == ESP_OK &&
        buffer[0] != '\0') {
        ESP_LOGI(kTag, "Using config from microSD deck.json");
        return ESP_OK;
    }

    /* 3. Embedded default. */
    std::snprintf(buffer, buffer_size, "%s", kDefaultJson);
    ESP_LOGI(kTag, "Using embedded default config");
    return ESP_OK;
}

esp_err_t deck_config_load(DeckConfig &config)
{
    static char buffer[kJsonBufferSize] = {};

    const esp_err_t err = deck_config_json(buffer, sizeof(buffer));
    if (err != ESP_OK) {
        return err;
    }

    if (!parse_json(buffer, config)) {
        ESP_LOGW(kTag, "Failed to parse config JSON; using embedded default");
        if (!parse_json(kDefaultJson, config)) {
            return ESP_FAIL;
        }
    }

    ESP_LOGI(kTag, "Loaded deck '%s': %u page(s)", config.name.c_str(),
             static_cast<unsigned>(config.pages.size()));
    return ESP_OK;
}
