#include "sticky_imu.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "app_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

namespace {

constexpr char kTag[] = "sticky_imu";
constexpr uint32_t kI2cClockHz = 400000;
constexpr int kI2cTimeoutMs = 100;
constexpr uint8_t kWhoAmIRegister = 0x0F;
constexpr uint8_t kExpectedWhoAmI = 0x6A;
constexpr uint8_t kAccelerometerControlRegister = 0x10;
constexpr uint8_t kControlRegister3 = 0x12;
constexpr uint8_t kAccelerometerOutputRegister = 0x28;
constexpr uint8_t kAccelerometer104Hz2g = 0x40;
constexpr uint8_t kRegisterAutoIncrement = 0x04;
constexpr float kAccelerationScaleG = 0.000061F;
constexpr float kOrientationThresholdG = 0.70F;
constexpr int kStableSampleCount = 5;
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(100);
constexpr uint32_t kTaskStackSize = 3072;
constexpr UBaseType_t kTaskPriority = 4;

i2c_master_dev_handle_t s_device = nullptr;
TaskHandle_t s_monitor_task = nullptr;
portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
StickyImuState s_latest_state = {};

esp_err_t write_register(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(
        s_device, data, sizeof(data), kI2cTimeoutMs);
}

esp_err_t read_registers(uint8_t reg, uint8_t *data, size_t length)
{
    return i2c_master_transmit_receive(
        s_device, &reg, 1, data, length, kI2cTimeoutMs);
}

int16_t read_int16_le(const uint8_t *data)
{
    return static_cast<int16_t>(
        (static_cast<uint16_t>(data[1]) << 8) | data[0]);
}

StickyImuOrientation classify_orientation(float x, float y, float z)
{
    const float abs_x = std::fabs(x);
    const float abs_y = std::fabs(y);
    const float abs_z = std::fabs(z);

    if (abs_z >= kOrientationThresholdG && abs_z > abs_x && abs_z > abs_y) {
        return z >= 0.0F ? StickyImuOrientation::FaceUp
                         : StickyImuOrientation::FaceDown;
    }
    if (abs_x >= kOrientationThresholdG && abs_x > abs_y) {
        // Calibration: -X is the normal landscape down direction.
        return x < 0.0F ? StickyImuOrientation::ArrowDown
                        : StickyImuOrientation::ArrowUp;
    }
    if (abs_y >= kOrientationThresholdG) {
        // Calibration: portrait -Y points toward the physical ground.
        return y < 0.0F ? StickyImuOrientation::ArrowRight
                        : StickyImuOrientation::ArrowLeft;
    }
    return StickyImuOrientation::Unknown;
}

esp_err_t read_acceleration(StickyImuState &state)
{
    uint8_t data[6] = {};
    const esp_err_t result =
        read_registers(kAccelerometerOutputRegister, data, sizeof(data));
    if (result != ESP_OK) {
        return result;
    }

    state.acceleration_x_g = read_int16_le(data) * kAccelerationScaleG;
    state.acceleration_y_g = read_int16_le(data + 2) * kAccelerationScaleG;
    state.acceleration_z_g = read_int16_le(data + 4) * kAccelerationScaleG;
    state.orientation = classify_orientation(state.acceleration_x_g,
                                             state.acceleration_y_g,
                                             state.acceleration_z_g);
    state.valid = true;
    return ESP_OK;
}

void store_state(const StickyImuState &state)
{
    taskENTER_CRITICAL(&s_state_lock);
    s_latest_state = state;
    taskEXIT_CRITICAL(&s_state_lock);
}

void monitor_task(void *)
{
    StickyImuOrientation candidate = StickyImuOrientation::Unknown;
    StickyImuOrientation stable = StickyImuOrientation::Unknown;
    int candidate_count = 0;
    TickType_t next_read = xTaskGetTickCount();

    while (true) {
        StickyImuState sample = {};
        const esp_err_t result = read_acceleration(sample);
        if (result == ESP_OK) {
            if (sample.orientation == candidate) {
                ++candidate_count;
            } else {
                candidate = sample.orientation;
                candidate_count = 1;
            }

            if (candidate_count >= kStableSampleCount && candidate != stable) {
                stable = candidate;
                sample.orientation = stable;
                store_state(sample);
                app_event_post(AppEvent::OrientationChanged);
            } else {
                sample.orientation = stable;
                store_state(sample);
            }
        } else {
            ESP_LOGE(kTag, "Accelerometer read failed: %s",
                     esp_err_to_name(result));
        }

        vTaskDelayUntil(&next_read, kPollInterval);
    }
}

}  // namespace

esp_err_t sticky_imu_init(i2c_master_bus_handle_t bus)
{
    if (bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_device != nullptr) {
        return ESP_OK;
    }

    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = LSM6DS3_I2C_ADDR;
    config.scl_speed_hz = kI2cClockHz;

    esp_err_t result = i2c_master_bus_add_device(bus, &config, &s_device);
    if (result != ESP_OK) {
        return result;
    }

    uint8_t who_am_i = 0;
    result = read_registers(kWhoAmIRegister, &who_am_i, 1);
    if (result != ESP_OK || who_am_i != kExpectedWhoAmI) {
        ESP_LOGE(kTag, "Unexpected WHO_AM_I: 0x%02X", who_am_i);
        i2c_master_bus_rm_device(s_device);
        s_device = nullptr;
        return result != ESP_OK ? result : ESP_ERR_NOT_FOUND;
    }

    result = write_register(
        kAccelerometerControlRegister, kAccelerometer104Hz2g);
    if (result == ESP_OK) {
        result = write_register(kControlRegister3, kRegisterAutoIncrement);
    }
    if (result != ESP_OK) {
        i2c_master_bus_rm_device(s_device);
        s_device = nullptr;
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(kTag, "Initialized: address=0x%02X, accelerometer=104Hz +/-2g",
             LSM6DS3_I2C_ADDR);
    return ESP_OK;
}

esp_err_t sticky_imu_start_monitoring()
{
    if (s_device == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_monitor_task != nullptr) {
        return ESP_OK;
    }

    return xTaskCreate(monitor_task,
                       "imu_monitor",
                       kTaskStackSize,
                       nullptr,
                       kTaskPriority,
                       &s_monitor_task) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}

esp_err_t sticky_imu_get_state(StickyImuState &state)
{
    taskENTER_CRITICAL(&s_state_lock);
    state = s_latest_state;
    taskEXIT_CRITICAL(&s_state_lock);
    return state.valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}
