#include "deck_app.h"

#include "app_event.h"
#include "ble_hid.h"
#include "board.h"
#include "canvas.h"
#include "deck_actions.h"
#include "deck_config.h"
#include "deck_render.h"
#include "esp_log.h"
#include "sticky_buttons.h"
#include "sticky_buzzer.h"
#include "sticky_display.h"
#include "sticky_power.h"
#include "sticky_sdcard.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "deck";

int find_page_index(const DeckConfig &config, const char *id)
{
    for (size_t i = 0; i < config.pages.size(); ++i) {
        if (config.pages[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void navigate(const DeckConfig &config, int &page_index, int direction)
{
    const int count = static_cast<int>(config.pages.size());
    if (count == 0) {
        return;
    }
    page_index = (page_index + direction + count) % count;
}

void render_current(Canvas &canvas, const DeckConfig &config, int page_index)
{
    if (page_index < 0 ||
        page_index >= static_cast<int>(config.pages.size())) {
        return;
    }
    deck_render_page(canvas, config.pages[static_cast<size_t>(page_index)]);
    ESP_ERROR_CHECK(sticky_display_refresh_monochrome());
}

}  // namespace

esp_err_t deck_app_run()
{
    ESP_LOGI(kTag, "StickyDeck starting");

    ESP_ERROR_CHECK(board_init());
    ESP_ERROR_CHECK(sticky_display_init());
    ESP_ERROR_CHECK(sticky_sdcard_init());
    ESP_ERROR_CHECK(app_event_init());
    ESP_ERROR_CHECK(sticky_buttons_init());
    ESP_ERROR_CHECK(sticky_touch_init());
    ESP_ERROR_CHECK(sticky_buzzer_init());

    /* BLE HID init is non-fatal: the deck still works without a host. */
    const esp_err_t ble_err = ble_hid_init();
    if (ble_err != ESP_OK) {
        ESP_LOGW(kTag, "BLE HID init failed: %s", esp_err_to_name(ble_err));
    }

    DeckConfig config;
    ESP_ERROR_CHECK(deck_config_load(config));
    ESP_LOGI(kTag, "Deck '%s': %u page(s)", config.name.c_str(),
             static_cast<unsigned>(config.pages.size()));

    Canvas *canvas = sticky_display_canvas();
    if (canvas == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    int page_index = 0;
    int highlight = -1;
    render_current(*canvas, config, page_index);

    while (true) {
        AppEventMessage msg = {};
        if (!app_event_wait_msg(msg, portMAX_DELAY)) {
            continue;
        }

        const DeckPage *page =
            (page_index >= 0 &&
             page_index < static_cast<int>(config.pages.size()))
                ? &config.pages[static_cast<size_t>(page_index)]
                : nullptr;

        switch (msg.event) {
        case AppEvent::PreviousPage:
            navigate(config, page_index, -1);
            highlight = -1;
            render_current(*canvas, config, page_index);
            break;

        case AppEvent::NextPage:
            navigate(config, page_index, +1);
            highlight = -1;
            render_current(*canvas, config, page_index);
            break;

        case AppEvent::RefreshPage:
            highlight = -1;
            render_current(*canvas, config, page_index);
            break;

        case AppEvent::TouchDown: {
            if (page == nullptr) {
                break;
            }
            const int index = deck_hit_test(*page, msg.x, msg.y);
            if (index < 0) {
                break;
            }
            const DeckAction &action =
                page->buttons[static_cast<size_t>(index)].action;

            if (action.type == DeckActionType::Page) {
                const int target =
                    find_page_index(config, action.page_target.c_str());
                if (target >= 0) {
                    page_index = target;
                    highlight = -1;
                    render_current(*canvas, config, page_index);
                }
            } else {
                const esp_err_t err = deck_action_execute(action);
                if (err != ESP_OK) {
                    ESP_LOGW(kTag, "action '%s' failed: %s",
                             page->buttons[static_cast<size_t>(index)]
                                 .label.c_str(),
                             esp_err_to_name(err));
                }
                sticky_buzzer_beep();
                deck_render_button(*canvas, *page, index, true);
                sticky_display_refresh_partial();
                highlight = index;
            }
            break;
        }

        case AppEvent::TouchUp:
            if (page != nullptr && highlight >= 0) {
                deck_render_button(*canvas, *page, highlight, false);
                sticky_display_refresh_partial();
            }
            highlight = -1;
            break;

        case AppEvent::TouchTap:
            /* TouchUp already restored the button highlight. */
            break;

        case AppEvent::EnterDeepSleep:
            ESP_LOGI(kTag, "Entering deep sleep");
            sticky_buzzer_beep();
            sticky_touch_stop();
            sticky_display_sleep();
            sticky_power_enter_deep_sleep(); /* does not return */
            break;

        default:
            break;
        }
    }
}
