/*
 * Minimal BLE HID test harness for the reTerminal Sticky.
 *
 * Boots the ble_hid component, advertises as "StickyDeck", and — when the AI
 * button (GPIO4) is pressed while a host is connected — sends a test text
 * string, a plain key, and a media key.
 */

#include "ble_hid.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";
#define PIN_AI GPIO_NUM_4

extern "C" void app_main()
{
    ESP_LOGI(TAG, "StickyDeck BLE HID test starting");

    ESP_ERROR_CHECK(ble_hid_init());

    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << PIN_AI;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&cfg);

    bool was_connected = false;
    bool auto_sent = false;
    TickType_t connected_at = 0;
    int last_level = gpio_get_level(PIN_AI);

    while (true) {
        bool connected = ble_hid_connected();
        if (connected != was_connected) {
            ESP_LOGI(TAG, "BLE connection state: %s", connected ? "CONNECTED" : "DISCONNECTED");
            was_connected = connected;
            if (connected) {
                connected_at = xTaskGetTickCount();
                auto_sent = false;
            }
        }

        /* Auto-send a test string 2 s after connect (so we can verify typing). */
        if (connected && !auto_sent &&
            (xTaskGetTickCount() - connected_at) >= pdMS_TO_TICKS(2000)) {
            auto_sent = true;
            ESP_LOGI(TAG, "auto-sending test text");
            ESP_ERROR_CHECK(ble_hid_type_text("Hello from StickyDeck! "));
            ESP_ERROR_CHECK(ble_hid_tap_media(BLE_HID_MEDIA_VOLUME_UP));
        }

        int level = gpio_get_level(PIN_AI);
        if (last_level == 1 && level == 0) { /* falling edge = button press */
            if (connected) {
                ESP_LOGI(TAG, "AI pressed -> sending test actions");
                ESP_ERROR_CHECK(ble_hid_type_text("Hello from StickyDeck! "));
                ESP_ERROR_CHECK(ble_hid_tap_key(0, 0x2C));                  /* space */
                ESP_ERROR_CHECK(ble_hid_tap_media(BLE_HID_MEDIA_VOLUME_UP));
            } else {
                ESP_LOGW(TAG, "AI pressed but no BLE host connected");
            }
        }
        last_level = level;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
