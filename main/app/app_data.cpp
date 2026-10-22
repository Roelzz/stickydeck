#include "app_data.h"

#include "app_state.h"
#include "esp_log.h"
#include "sticky_battery.h"
#include "sticky_charger.h"
#include "sticky_imu.h"
#include "sticky_microphone.h"
#include "sticky_rtc.h"
#include "sticky_sdcard.h"
#include "sticky_sht40.h"

namespace {

constexpr char kTag[] = "app_data";

const char *orientation_name(StickyImuOrientation orientation)
{
    switch (orientation) {
    case StickyImuOrientation::Unknown: return "Unknown";
    case StickyImuOrientation::ArrowUp: return "ArrowUp";
    case StickyImuOrientation::ArrowDown: return "ArrowDown";
    case StickyImuOrientation::ArrowLeft: return "ArrowLeft";
    case StickyImuOrientation::ArrowRight: return "ArrowRight";
    case StickyImuOrientation::FaceUp: return "FaceUp";
    case StickyImuOrientation::FaceDown: return "FaceDown";
    }
    return "Unknown";
}

}  // namespace

void update_date_time(AppState &state)
{
    RtcDateTime reading = {};
    const esp_err_t result = sticky_rtc_read(reading);

    state.date_time.error = result;
    state.date_time.valid = result == ESP_OK;
    if (result == ESP_OK) {
        state.date_time.year = reading.year;
        state.date_time.month = reading.month;
        state.date_time.day = reading.day;
        state.date_time.hour = reading.hour;
        state.date_time.minute = reading.minute;
        state.date_time.second = reading.second;
    } else {
        ESP_LOGW(kTag, "RTC update failed: %s; status bar will show --:--",
                 esp_err_to_name(result));
    }
}

void update_environment(AppState &state)
{
    ESP_LOGI(kTag, "Updating environment data");
    Sht40Reading reading = {};
    const esp_err_t result = sticky_sht40_read(reading);

    state.environment.error = result;
    state.environment.valid = result == ESP_OK;
    if (result == ESP_OK) {
        state.environment.temperature_c = reading.temperature_c;
        state.environment.humidity_percent = reading.humidity_percent;
        ESP_LOGI(kTag, "State updated: temperature=%.2f C, humidity=%.2f %%",
                 static_cast<double>(state.environment.temperature_c),
                 static_cast<double>(state.environment.humidity_percent));
    } else {
        ESP_LOGE(kTag, "Environment update failed: %s; page will show N/A",
                 esp_err_to_name(result));
    }
}

void update_battery(AppState &state)
{
    BatteryReading reading = {};
    const esp_err_t battery_result = sticky_battery_read(reading);
    state.battery.error = battery_result;
    state.battery.valid = battery_result == ESP_OK;
    if (battery_result == ESP_OK) {
        state.battery.percent = reading.percent;
    }

    bool charging = false;
    const esp_err_t charger_result = sticky_charger_read(charging);
    state.battery.charging_error = charger_result;
    state.battery.charging_valid = charger_result == ESP_OK;
    if (charger_result == ESP_OK) {
        state.battery.charging = charging;
    }
}

void update_note(AppState &state)
{
    const esp_err_t result = sticky_sdcard_read_text(
        "TEST.TXT", state.note.text, sizeof(state.note.text));
    state.note.error = result;
    state.note.valid = result == ESP_OK;
    if (result != ESP_OK) {
        ESP_LOGW(kTag, "Note load failed: %s", esp_err_to_name(result));
    }
}

void update_imu(AppState &state)
{
    StickyImuState reading = {};
    const esp_err_t result = sticky_imu_get_state(reading);
    state.imu.error = result;
    state.imu.valid = result == ESP_OK;
    if (result == ESP_OK) {
        state.imu.orientation = reading.orientation;
        ESP_LOGI(kTag,
                 "IMU: orientation=%s, acceleration=(%.2f,%.2f,%.2f)g",
                 orientation_name(reading.orientation),
                 static_cast<double>(reading.acceleration_x_g),
                 static_cast<double>(reading.acceleration_y_g),
                 static_cast<double>(reading.acceleration_z_g));
    }
}

void update_microphone(AppState &state)
{
    MicrophoneReading reading = {};
    const esp_err_t result = sticky_microphone_capture(reading);
    state.microphone.captured = true;
    state.microphone.error = result;
    state.microphone.valid = result == ESP_OK;
    if (result == ESP_OK) {
        state.microphone.rms = reading.rms;
        state.microphone.peak = reading.peak;
        state.microphone.level_percent = reading.level_percent;
    }
}
