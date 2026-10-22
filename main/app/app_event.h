#pragma once

#include <cstdint>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

enum class AppEvent {
    PreviousPage,
    NextPage,
    RefreshPage,
    OrientationChanged,
    ClockMinuteTick,
    EnterDeepSleep,
    TouchDown,
    TouchUp,
    TouchTap,
};

/* Queue message: an event plus optional touch coordinates (in physical
 * 800x480 display coordinates). Non-touch events carry x = y = 0. */
struct AppEventMessage {
    AppEvent event;
    uint16_t x;
    uint16_t y;
};

// Small shared queue used by physical buttons and touch input.
esp_err_t app_event_init();
bool app_event_post(AppEvent event);
bool app_event_post_touch(AppEvent event, uint16_t x, uint16_t y);
bool app_event_wait(AppEvent &event, TickType_t timeout);
bool app_event_wait_msg(AppEventMessage &msg, TickType_t timeout);
