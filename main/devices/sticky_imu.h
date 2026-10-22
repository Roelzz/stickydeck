#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

enum class StickyImuOrientation {
    Unknown,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    FaceUp,
    FaceDown,
};

struct StickyImuState {
    float acceleration_x_g = 0.0F;
    float acceleration_y_g = 0.0F;
    float acceleration_z_g = 0.0F;
    StickyImuOrientation orientation = StickyImuOrientation::Unknown;
    bool valid = false;
};

// Initializes the LSM6DS3TR-C accelerometer on the shared sensor I2C bus.
esp_err_t sticky_imu_init(i2c_master_bus_handle_t bus);

// Starts stable-orientation monitoring and posts AppEvent::OrientationChanged.
esp_err_t sticky_imu_start_monitoring();

// Copies the latest stable state collected by the monitoring task.
esp_err_t sticky_imu_get_state(StickyImuState &state);
