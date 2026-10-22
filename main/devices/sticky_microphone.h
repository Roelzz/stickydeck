#pragma once

#include "esp_err.h"

struct MicrophoneReading {
    float rms = 0.0F;
    int peak = 0;
    int level_percent = 0;
};

// Initializes the PDM receive channel while leaving microphone power off.
esp_err_t sticky_microphone_init();

// Powers the microphone, captures about one second, then powers it off.
esp_err_t sticky_microphone_capture(MicrophoneReading &reading);
