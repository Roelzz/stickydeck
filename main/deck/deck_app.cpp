#include "deck_app.h"

#include "app_event.h"
#include "ble_hid.h"
#include "board.h"
#include "canvas.h"
#include "deck_actions.h"
#include "deck_config.h"
#include "deck_render.h"
#include "esp_log.h"
#include "sticky_battery.h"
#include "sticky_buttons.h"
#include "sticky_buzzer.h"
#include "sticky_display.h"
#include "sticky_power.h"
#include "sticky_sdcard.h"
#include "sticky_touch.h"
#include "webserver.h"

namespace {

constexpr char kTag[] = "deck";
constexpr TickType_t kStatusPollPeriod = pdMS_TO_TICKS(10000);

bool s_config_error = false;

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

DeckStatus read_status()
{
    DeckStatus status;
    status.ble = ble_hid_connected();
    status.wifi = deck_webserver_is_running();
    status.error = s_config_error;

    BatteryReading reading = {};
    if (sticky_battery_read(reading) == ESP_OK) {
        status.battery = reading.percent;
    }
    return status;
}

void render_full(Canvas &canvas, const DeckConfig &config, int page_index,
                 DeckStatus &last_status)
{
    if (page_index < 0 ||
        page_index >= static_cast<int>(config.pages.size())) {
        return;
    }
    const DeckStatus status = read_status();
    deck_render_page(canvas, config.pages[static_cast<size_t>(page_index)],
                     &status);
    ESP_ERROR_CHECK(sticky_display_refresh_monochrome());
    last_status = status;
}

void render_status_if_changed(Canvas &canvas, DeckStatus &last_status)
{
    const DeckStatus status = read_status();

    /* Push battery level over the HID battery service. */
    if (status.battery >= 0 && status.battery <= 100) {
        ble_hid_set_battery(static_cast<uint8_t>(status.battery));
    }

    if (status.battery == last_status.battery &&
        status.ble == last_status.ble && status.wifi == last_status.wifi &&
        status.error == last_status.error) {
        return;
    }
    deck_render_status(canvas, status);
    sticky_display_refresh_partial();
    last_status = status;
    ESP_LOGI(kTag, "status: battery=%d ble=%d wifi=%d", status.battery,
             static_cast<int>(status.ble), static_cast<int>(status.wifi));
}

[[noreturn]] void enter_deep_sleep(Canvas &canvas)
{
    const DeckStatus status = read_status();
    deck_render_sleep(canvas, status);
    ESP_ERROR_CHECK(sticky_display_refresh_monochrome());
    sticky_touch_stop();
    sticky_display_sleep();
    sticky_power_enter_deep_sleep();
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

    /* Battery gauge is optional: the deck still works without a valid read. */
    const esp_err_t battery_err = sticky_battery_init(board_sensor_i2c_bus());
    if (battery_err != ESP_OK) {
        ESP_LOGW(kTag, "battery init failed: %s", esp_err_to_name(battery_err));
    }

    /* BLE HID init is non-fatal: the deck still works without a host. */
    const esp_err_t ble_err = ble_hid_init();
    if (ble_err != ESP_OK) {
        ESP_LOGW(kTag, "BLE HID init failed: %s", esp_err_to_name(ble_err));
    }

    DeckConfig config;
    ESP_ERROR_CHECK(deck_config_load(config));
    ESP_LOGI(kTag, "Deck '%s': %u page(s)", config.name.c_str(),
             static_cast<unsigned>(config.pages.size()));
    s_config_error = config.fallback;
    deck_webserver_set_ap_password(config.ap_password.c_str());

    /* Wi-Fi config server: controlled by config.wifi; non-fatal. */
    if (config.wifi) {
        const esp_err_t web_err = deck_webserver_start();
        if (web_err != ESP_OK) {
            ESP_LOGW(kTag, "web server start failed: %s",
                     esp_err_to_name(web_err));
        }
    } else {
        ESP_LOGI(kTag, "Wi-Fi config AP disabled by config");
    }

    Canvas *canvas = sticky_display_canvas();
    if (canvas == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    int page_index = 0;
    int highlight = -1;
    DeckStatus last_status = {};
    TickType_t last_activity = xTaskGetTickCount();
    render_full(*canvas, config, page_index, last_status);

    while (true) {
        AppEventMessage msg = {};
        if (!app_event_wait_msg(msg, kStatusPollPeriod)) {
            /* Periodic status update (battery / BLE indicator). */
            render_status_if_changed(*canvas, last_status);

            /* Auto-sleep after an idle timeout. */
            if (config.sleep_minutes > 0 &&
                (xTaskGetTickCount() - last_activity) >= pdMS_TO_TICKS(
                    static_cast<uint32_t>(config.sleep_minutes) * 60 * 1000)) {
                ESP_LOGI(kTag, "Idle %d min, entering deep sleep",
                         config.sleep_minutes);
                enter_deep_sleep(*canvas);
            }
            continue;
        }

        last_activity = xTaskGetTickCount();

        const DeckPage *page =
            (page_index >= 0 &&
             page_index < static_cast<int>(config.pages.size()))
                ? &config.pages[static_cast<size_t>(page_index)]
                : nullptr;

        switch (msg.event) {
        case AppEvent::PreviousPage:
            navigate(config, page_index, -1);
            highlight = -1;
            render_full(*canvas, config, page_index, last_status);
            break;

        case AppEvent::NextPage:
            navigate(config, page_index, +1);
            highlight = -1;
            render_full(*canvas, config, page_index, last_status);
            break;

        case AppEvent::RefreshPage:
            highlight = -1;
            render_full(*canvas, config, page_index, last_status);
            break;

        case AppEvent::TouchDown: {
            if (deck_header_hit(msg.x, msg.y)) {
                /* Tapping the header bar toggles the Wi-Fi config AP. */
                const esp_err_t err = deck_webserver_toggle();
                if (err != ESP_OK) {
                    ESP_LOGW(kTag, "wifi toggle failed: %s",
                             esp_err_to_name(err));
                }
                if (config.beep) {
                    sticky_buzzer_beep();
                }
                render_status_if_changed(*canvas, last_status);
                ESP_LOGI(kTag, "Wi-Fi AP now %s",
                         deck_webserver_is_running() ? "on" : "off");
                break;
            }
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
                    render_full(*canvas, config, page_index, last_status);
                }
            } else if (action.type == DeckActionType::Wifi) {
                const esp_err_t err = deck_webserver_toggle();
                if (err != ESP_OK) {
                    ESP_LOGW(kTag, "wifi toggle failed: %s",
                             esp_err_to_name(err));
                }
                if (config.beep) {
                    sticky_buzzer_beep();
                }
                ESP_LOGI(kTag, "Wi-Fi AP now %s",
                         deck_webserver_is_running() ? "on" : "off");
            } else {
                const esp_err_t err = deck_action_execute(action);
                if (err != ESP_OK) {
                    ESP_LOGW(kTag, "action '%s' failed: %s",
                             page->buttons[static_cast<size_t>(index)]
                                 .label.c_str(),
                             esp_err_to_name(err));
                } else {
                    ESP_LOGI(kTag, "button '%s' fired",
                             page->buttons[static_cast<size_t>(index)]
                                 .label.c_str());
                }
                if (config.beep) {
                    sticky_buzzer_beep();
                }
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
            enter_deep_sleep(*canvas);
            break;

        default:
            break;
        }
    }
}
