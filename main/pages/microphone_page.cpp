#include "microphone_page.h"

#include <cstdio>

#include "app_state.h"
#include "canvas.h"
#include "page.h"

void microphone_page_render(Canvas &canvas, const AppState &state)
{
    page_draw_layout(canvas, state, "Hello Sticky", "Microphone Page");

    canvas.draw_text(76, 220, "Sound Level", 3, GrayLevel::Black);
    canvas.draw_rect(76, 270, 560, 54, GrayLevel::Black);

    if (state.microphone.valid) {
        const int fill_width = state.microphone.level_percent * 548 / 100;
        canvas.fill_rect(82, 276, fill_width, 42, GrayLevel::Black);

        char value[32] = {};
        std::snprintf(value, sizeof(value), "RMS: %.1f",
                      static_cast<double>(state.microphone.rms));
        canvas.draw_text(76, 350, value, 2, GrayLevel::Black);
        std::snprintf(value, sizeof(value), "Peak: %d", state.microphone.peak);
        canvas.draw_text(390, 350, value, 2, GrayLevel::Black);
    } else if (state.microphone.captured) {
        canvas.draw_text(76, 350, "Microphone capture failed",
                         2, GrayLevel::Black);
    }

    canvas.draw_text(76, 400, "Press AI button to sample",
                     2, GrayLevel::Black);
}
