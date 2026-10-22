#include "app.h"
#include "app_clock.h"
#include "app_data.h"
#include "board.h"
#include "canvas.h"
#include "esp_log.h"
#include "sticky_buzzer.h"
#include "sticky_battery.h"
#include "sticky_buttons.h"
#include "sticky_charger.h"
#include "sticky_display.h"
#include "sticky_imu.h"
#include "sticky_microphone.h"
#include "sticky_power.h"
#include "sticky_rtc.h"
#include "sticky_sdcard.h"
#include "sticky_sht40.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "sticky_dashboard";

void show_initial_page(Canvas &canvas, const AppState &state)
{
    render_current_page(canvas, state);
    ESP_ERROR_CHECK(refresh_current_page(state));
}

}  // namespace

extern "C" void app_main()
{
    ESP_LOGI(kTag, "Starting reTerminal Sticky Dashboard Demo");
    sticky_power_log_wakeup_reason();

    ESP_ERROR_CHECK(board_init());
    ESP_ERROR_CHECK(sticky_display_init());
    ESP_ERROR_CHECK(sticky_sdcard_init());
    ESP_ERROR_CHECK(app_event_init());
    ESP_ERROR_CHECK(sticky_buttons_init());
    ESP_ERROR_CHECK(sticky_touch_init());
    ESP_ERROR_CHECK(sticky_buzzer_init());
    ESP_ERROR_CHECK(sticky_charger_init());
    ESP_ERROR_CHECK(sticky_battery_init(board_sensor_i2c_bus()));
    ESP_ERROR_CHECK(sticky_sht40_init(board_sensor_i2c_bus()));
    ESP_ERROR_CHECK(sticky_rtc_init(board_sensor_i2c_bus()));
    ESP_ERROR_CHECK(sticky_imu_init(board_sensor_i2c_bus()));
    ESP_ERROR_CHECK(sticky_microphone_init());

    Canvas *canvas = sticky_display_canvas();
    ESP_ERROR_CHECK(canvas != nullptr ? ESP_OK : ESP_ERR_INVALID_STATE);

    AppState state;
    restore_app_state_after_deep_sleep(state);
    update_date_time(state);
    show_initial_page(*canvas, state);
    ESP_ERROR_CHECK(
        app_clock_start(state.date_time.valid ? state.date_time.second : -1));
    ESP_ERROR_CHECK(sticky_imu_start_monitoring());

    while (true) {
        AppEvent event;
        if (!app_event_wait(event, portMAX_DELAY)) {
            continue;
        }

        const esp_err_t result = handle_app_event(*canvas, state, event);
        if (result != ESP_OK) {
            ESP_LOGE(kTag, "App event failed: %s", esp_err_to_name(result));
        }
    }
}
