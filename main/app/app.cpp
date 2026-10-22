#include "app.h"

#include <cstdint>
#include <cstring>

#include "app_data.h"
#include "canvas.h"
#include "battery_page.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "home_page.h"
#include "imu_page.h"
#include "microphone_page.h"
#include "note_page.h"
#include "page.h"
#include "sensor_page.h"
#include "sleep_page.h"
#include "sticky_buzzer.h"
#include "sticky_display.h"
#include "sticky_power.h"
#include "sticky_touch.h"
#include "status_bar.h"

namespace {

constexpr char kTag[] = "app";
constexpr uint32_t kSavedStateMagic = 0x53544943U;  // "STIC"

RTC_NOINIT_ATTR uint32_t s_saved_state_magic;
RTC_NOINIT_ATTR uint8_t s_saved_state[sizeof(AppState)];

void save_app_state_for_deep_sleep(const AppState &state)
{
    std::memcpy(s_saved_state, &state, sizeof(state));
    s_saved_state_magic = kSavedStateMagic;
}

const char *page_name(PageId page)
{
    switch (page) {
    case PageId::Home:
        return "Home";
    case PageId::Sensor:
        return "Sensor";
    case PageId::Battery:
        return "Battery";
    case PageId::Note:
        return "Note";
    case PageId::Imu:
        return "IMU";
    case PageId::Microphone:
        return "Microphone";
    }
    return "Unknown";
}

bool page_uses_gray4(PageId page)
{
    return page == PageId::Home;
}

}  // namespace

esp_err_t refresh_current_page(const AppState &state)
{
    return page_uses_gray4(state.current_page)
               ? sticky_display_refresh()
               : sticky_display_refresh_monochrome();
}

bool restore_app_state_after_deep_sleep(AppState &state)
{
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1 ||
        s_saved_state_magic != kSavedStateMagic) {
        return false;
    }

    std::memcpy(&state, s_saved_state, sizeof(state));
    s_saved_state_magic = 0;
    ESP_LOGI(kTag, "Restored %s Page state after deep sleep",
             page_name(state.current_page));
    return true;
}

void render_current_page(Canvas &canvas, const AppState &state)
{
    PageRenderer renderer = nullptr;

    switch (state.current_page) {
    case PageId::Home:
        renderer = home_page_render;
        break;
    case PageId::Sensor:
        renderer = sensor_page_render;
        break;
    case PageId::Battery:
        renderer = battery_page_render;
        break;
    case PageId::Note:
        renderer = note_page_render;
        break;
    case PageId::Imu:
        renderer = imu_page_render;
        break;
    case PageId::Microphone:
        renderer = microphone_page_render;
        break;
    }

    if (renderer != nullptr) {
        renderer(canvas, state);
    }
}

void select_previous_page(AppState &state)
{
    switch (state.current_page) {
    case PageId::Home:
        state.current_page = PageId::Microphone;
        break;
    case PageId::Sensor:
        state.current_page = PageId::Home;
        break;
    case PageId::Battery:
        state.current_page = PageId::Sensor;
        break;
    case PageId::Note:
        state.current_page = PageId::Battery;
        break;
    case PageId::Imu:
        state.current_page = PageId::Note;
        break;
    case PageId::Microphone:
        state.current_page = PageId::Imu;
        break;
    }
}

void select_next_page(AppState &state)
{
    switch (state.current_page) {
    case PageId::Home:
        state.current_page = PageId::Sensor;
        break;
    case PageId::Sensor:
        state.current_page = PageId::Battery;
        break;
    case PageId::Battery:
        state.current_page = PageId::Note;
        break;
    case PageId::Note:
        state.current_page = PageId::Imu;
        break;
    case PageId::Imu:
        state.current_page = PageId::Microphone;
        break;
    case PageId::Microphone:
        state.current_page = PageId::Home;
        break;
    }
}

esp_err_t handle_app_event(Canvas &canvas,
                           AppState &state,
                           AppEvent event)
{
    if (event == AppEvent::ClockMinuteTick) {
        const bool previous_valid = state.date_time.valid;
        const int previous_hour = state.date_time.hour;
        const int previous_minute = state.date_time.minute;

        update_date_time(state);
        const bool time_changed =
            previous_valid != state.date_time.valid ||
            (state.date_time.valid &&
             (previous_hour != state.date_time.hour ||
              previous_minute != state.date_time.minute));
        if (!time_changed) {
            return ESP_OK;
        }

        // Home is the gray4 demonstration page and has no RTC status bar.
        if (page_uses_gray4(state.current_page)) {
            ESP_LOGI(kTag, "Skipping RTC partial refresh on gray4 Home Page");
            return ESP_OK;
        }

        status_bar_render_time(canvas, state);
        ESP_LOGI(kTag, "Partially refreshing RTC time");
        return sticky_display_refresh_partial();
    }

    const PageId previous_page = state.current_page;
    const bool orientation_event = event == AppEvent::OrientationChanged;

    if (event == AppEvent::EnterDeepSleep) {
        ESP_LOGI(kTag, "Preparing for deep sleep");
        const esp_err_t beep_result = sticky_buzzer_beep();
        if (beep_result != ESP_OK) {
            ESP_LOGW(kTag, "Deep-sleep feedback beep failed: %s",
                     esp_err_to_name(beep_result));
        }
        save_app_state_for_deep_sleep(state);
        sleep_page_render(canvas);
        ESP_RETURN_ON_ERROR(
            sticky_display_refresh_monochrome(),
            "app", "refresh deep sleep page");
        ESP_RETURN_ON_ERROR(
            sticky_touch_stop(), "app", "stop touch polling");
        ESP_RETURN_ON_ERROR(
            sticky_display_sleep(), "app", "sleep display");
        sticky_power_enter_deep_sleep();
    }

    if (orientation_event && state.current_page != PageId::Imu) {
        return ESP_OK;
    }

    switch (event) {
    case AppEvent::PreviousPage:
        select_previous_page(state);
        break;
    case AppEvent::NextPage:
        select_next_page(state);
        break;
    case AppEvent::RefreshPage:
    case AppEvent::OrientationChanged:
    case AppEvent::ClockMinuteTick:
        break;
    case AppEvent::EnterDeepSleep:
        // Handled before this switch because deep sleep does not return.
        break;
    }

    if (state.current_page != previous_page) {
        ESP_LOGI(kTag, "Page changed: %s -> %s",
                 page_name(previous_page), page_name(state.current_page));
    } else {
        ESP_LOGI(kTag, "Refreshing %s Page", page_name(state.current_page));
    }

    ESP_LOGI(kTag, "Rendering %s Page", page_name(state.current_page));

    // RTC failure changes only the status bar; page rendering still proceeds.
    update_date_time(state);

    if (state.current_page == PageId::Sensor) {
        // A failed read is represented in AppState and rendered by Sensor Page.
        update_environment(state);
    } else if (state.current_page == PageId::Battery) {
        update_battery(state);
    } else if (state.current_page == PageId::Note) {
        // SD is unmounted before render_current_page() refreshes the display.
        update_note(state);
    } else if (state.current_page == PageId::Imu) {
        update_imu(state);
    } else if (state.current_page == PageId::Microphone &&
               event == AppEvent::RefreshPage) {
        update_microphone(state);
    }

    render_current_page(canvas, state);
    ESP_RETURN_ON_ERROR(
        refresh_current_page(state), "app", "refresh current page");
    ESP_LOGI(kTag, "%s Page display refresh completed",
             page_name(state.current_page));
    return orientation_event ? ESP_OK : sticky_buzzer_beep();
}
