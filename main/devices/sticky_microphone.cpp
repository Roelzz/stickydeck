#include "sticky_microphone.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "driver/gpio.h"
#include "driver/i2s_common.h"
#include "driver/i2s_pdm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

namespace {

constexpr char kTag[] = "sticky_microphone";
constexpr uint32_t kSampleRateHz = 16000;
constexpr size_t kCaptureSampleCount = kSampleRateHz;
constexpr size_t kSamplesPerRead = 512;
constexpr int kReadTimeoutMs = 1000;
constexpr int kPowerOnDelayMs = 20;
constexpr float kRmsForFullScaleBar = 8192.0F;

i2s_chan_handle_t s_rx_channel = nullptr;
bool s_capturing = false;

esp_err_t set_power(bool enabled)
{
    return gpio_set_level(
        static_cast<gpio_num_t>(PIN_MIC_EN), enabled ? 1 : 0);
}

}  // namespace

esp_err_t sticky_microphone_init()
{
    if (s_rx_channel != nullptr) {
        return ESP_OK;
    }

    gpio_config_t power_config = {};
    power_config.pin_bit_mask = 1ULL << PIN_MIC_EN;
    power_config.mode = GPIO_MODE_OUTPUT;
    power_config.pull_up_en = GPIO_PULLUP_DISABLE;
    power_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    power_config.intr_type = GPIO_INTR_DISABLE;
    esp_err_t result = gpio_config(&power_config);
    if (result != ESP_OK) {
        return result;
    }
    result = set_power(false);
    if (result != ESP_OK) {
        return result;
    }

    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    result = i2s_new_channel(&channel_config, nullptr, &s_rx_channel);
    if (result != ESP_OK) {
        s_rx_channel = nullptr;
        return result;
    }

    i2s_pdm_rx_config_t pdm_config = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(kSampleRateHz),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = static_cast<gpio_num_t>(PIN_MIC_CLK),
            .din = static_cast<gpio_num_t>(PIN_MIC_DATA),
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    result = i2s_channel_init_pdm_rx_mode(s_rx_channel, &pdm_config);
    if (result != ESP_OK) {
        i2s_del_channel(s_rx_channel);
        s_rx_channel = nullptr;
        return result;
    }

    ESP_LOGI(kTag, "Initialized: 16kHz, 16-bit mono PDM");
    return ESP_OK;
}

esp_err_t sticky_microphone_capture(MicrophoneReading &reading)
{
    if (s_rx_channel == nullptr || s_capturing) {
        return ESP_ERR_INVALID_STATE;
    }

    s_capturing = true;
    esp_err_t result = set_power(true);
    if (result == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(kPowerOnDelayMs));
        result = i2s_channel_enable(s_rx_channel);
    }

    double sum_centered_squares = 0.0;
    size_t processed_samples = 0;
    int peak = 0;
    int16_t samples[kSamplesPerRead] = {};

    while (result == ESP_OK && processed_samples < kCaptureSampleCount) {
        size_t bytes_read = 0;
        result = i2s_channel_read(s_rx_channel,
                                  samples,
                                  sizeof(samples),
                                  &bytes_read,
                                  pdMS_TO_TICKS(kReadTimeoutMs));
        const size_t sample_count = bytes_read / sizeof(samples[0]);
        if (result != ESP_OK || sample_count == 0) {
            if (result == ESP_OK) {
                result = ESP_ERR_TIMEOUT;
            }
            break;
        }

        double mean = 0.0;
        for (size_t index = 0; index < sample_count; ++index) {
            mean += samples[index];
        }
        mean /= static_cast<double>(sample_count);

        for (size_t index = 0; index < sample_count; ++index) {
            const double centered = samples[index] - mean;
            sum_centered_squares += centered * centered;
            peak = std::max(peak, static_cast<int>(std::fabs(centered)));
        }
        processed_samples += sample_count;
    }

    const esp_err_t disable_result = i2s_channel_disable(s_rx_channel);
    const esp_err_t power_result = set_power(false);
    s_capturing = false;

    if (result == ESP_OK && disable_result != ESP_OK) {
        result = disable_result;
    }
    if (result == ESP_OK && power_result != ESP_OK) {
        result = power_result;
    }
    if (result != ESP_OK || processed_samples == 0) {
        ESP_LOGE(kTag, "Capture failed: %s", esp_err_to_name(result));
        return result != ESP_OK ? result : ESP_FAIL;
    }

    reading.rms = static_cast<float>(
        std::sqrt(sum_centered_squares / processed_samples));
    reading.peak = peak;
    reading.level_percent = std::clamp(
        static_cast<int>(reading.rms * 100.0F / kRmsForFullScaleBar),
        0,
        100);

    ESP_LOGI(kTag, "Capture success: samples=%u RMS=%.1f peak=%d level=%d%%",
             static_cast<unsigned>(processed_samples),
             static_cast<double>(reading.rms),
             reading.peak,
             reading.level_percent);
    return ESP_OK;
}
