#pragma once

#include "esp_err.h"
#include "sticky_imu.h"

enum class PageId {
    Home,
    Sensor,
    Battery,
    Note,
    Imu,
    Microphone,
};

struct DateTimeState {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    bool valid = false;
    esp_err_t error = ESP_ERR_INVALID_STATE;
};

struct EnvironmentState {
    float temperature_c = 0.0F;
    float humidity_percent = 0.0F;
    bool valid = false;
    esp_err_t error = ESP_ERR_INVALID_STATE;
};

struct BatteryState {
    int percent = 0;
    bool valid = false;
    esp_err_t error = ESP_ERR_INVALID_STATE;
    bool charging = false;
    bool charging_valid = false;
    esp_err_t charging_error = ESP_ERR_INVALID_STATE;
};

struct NoteState {
    char text[257] = {};
    bool valid = false;
    esp_err_t error = ESP_ERR_INVALID_STATE;
};

struct ImuState {
    StickyImuOrientation orientation = StickyImuOrientation::Unknown;
    bool valid = false;
    esp_err_t error = ESP_ERR_INVALID_STATE;
};

struct MicrophoneState {
    float rms = 0.0F;
    int peak = 0;
    int level_percent = 0;
    bool captured = false;
    bool valid = false;
    esp_err_t error = ESP_ERR_INVALID_STATE;
};

struct AppState {
    PageId current_page = PageId::Home;
    DateTimeState date_time;
    EnvironmentState environment;
    BatteryState battery;
    NoteState note;
    ImuState imu;
    MicrophoneState microphone;
};
