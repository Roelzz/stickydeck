#include "app_event.h"

#include "freertos/queue.h"

namespace {

constexpr UBaseType_t kQueueLength = 16;
QueueHandle_t s_event_queue = nullptr;

bool post_msg(const AppEventMessage &msg)
{
    return s_event_queue != nullptr &&
           xQueueSend(s_event_queue, &msg, 0) == pdTRUE;
}

}  // namespace

esp_err_t app_event_init()
{
    if (s_event_queue != nullptr) {
        return ESP_OK;
    }

    s_event_queue = xQueueCreate(kQueueLength, sizeof(AppEventMessage));
    return s_event_queue != nullptr ? ESP_OK : ESP_ERR_NO_MEM;
}

bool app_event_post(AppEvent event)
{
    const AppEventMessage msg = {event, 0, 0};
    return post_msg(msg);
}

bool app_event_post_touch(AppEvent event, uint16_t x, uint16_t y)
{
    const AppEventMessage msg = {event, x, y};
    return post_msg(msg);
}

bool app_event_wait_msg(AppEventMessage &msg, TickType_t timeout)
{
    return s_event_queue != nullptr &&
           xQueueReceive(s_event_queue, &msg, timeout) == pdTRUE;
}

bool app_event_wait(AppEvent &event, TickType_t timeout)
{
    AppEventMessage msg = {};
    if (!app_event_wait_msg(msg, timeout)) {
        return false;
    }
    event = msg.event;
    return true;
}
