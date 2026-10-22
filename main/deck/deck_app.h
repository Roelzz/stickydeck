#pragma once

#include "esp_err.h"

/**
 * Initialize the hardware, BLE HID, and deck configuration, then run the
 * deck event loop. This function never returns.
 */
esp_err_t deck_app_run();
