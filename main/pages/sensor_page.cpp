#include "sensor_page.h"

#include <cstdio>

#include "app_state.h"
#include "canvas.h"
#include "page.h"

void sensor_page_render(Canvas &canvas, const AppState &state)
{
    char value[24] = {};

    page_draw_layout(canvas, state, "Hello Sticky", "Sensor Page");

    canvas.draw_text(76, 235, "Temperature:", 3, GrayLevel::Black);
    if (state.environment.valid) {
        std::snprintf(value, sizeof(value), "%.1f C",
                      static_cast<double>(state.environment.temperature_c));
        canvas.draw_text(500, 235, value, 3, GrayLevel::Black);
    } else {
        canvas.draw_text(500, 235, "N/A", 3, GrayLevel::Black);
    }

    canvas.draw_text(76, 325, "Humidity:", 3, GrayLevel::Black);
    if (state.environment.valid) {
        std::snprintf(value, sizeof(value), "%.1f %%",
                      static_cast<double>(state.environment.humidity_percent));
        canvas.draw_text(500, 325, value, 3, GrayLevel::Black);
    } else {
        canvas.draw_text(500, 325, "N/A", 3, GrayLevel::Black);
    }

    canvas.draw_line(76, 395, 718, 395, GrayLevel::Black);
}
