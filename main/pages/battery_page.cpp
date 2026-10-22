#include "battery_page.h"

#include <cstdio>

#include "app_state.h"
#include "canvas.h"
#include "page.h"

namespace {

void draw_lightning_icon(Canvas &canvas, int x, int y, int scale)
{
    constexpr uint8_t bolt[] = {
        0x06, 0x06, 0x0C, 0x0C, 0x18, 0x1F, 0x0E,
        0x06, 0x06, 0x0C, 0x0C, 0x08, 0x08,
    };

    for (int row = 0; row < 13; ++row) {
        for (int column = 0; column < 5; ++column) {
            if ((bolt[row] & (1U << (4 - column))) != 0U) {
                canvas.fill_rect(x + column * scale,
                                 y + row * scale,
                                 scale,
                                 scale,
                                 GrayLevel::Black);
            }
        }
    }
}

}  // namespace

void battery_page_render(Canvas &canvas, const AppState &state)
{
    page_draw_layout(canvas, state, "Hello Sticky", "Battery Page");

    constexpr int battery_x = 200;
    constexpr int battery_y = 220;
    constexpr int battery_width = 280;
    constexpr int battery_height = 120;
    constexpr int padding = 12;

    canvas.draw_rect(battery_x, battery_y,
                     battery_width, battery_height, GrayLevel::Black);
    canvas.fill_rect(battery_x + battery_width,
                     battery_y + 38, 18, 44, GrayLevel::Black);

    if (state.battery.valid) {
        const int inner_width = battery_width - padding * 2;
        const int fill_width = inner_width * state.battery.percent / 100;
        canvas.fill_rect(battery_x + padding,
                         battery_y + padding,
                         fill_width,
                         battery_height - padding * 2,
                         GrayLevel::Black);
    }

    if (state.battery.charging_valid && state.battery.charging) {
        draw_lightning_icon(canvas, 140, 250, 4);
    }

    char percent_text[8] = "--%";
    if (state.battery.valid) {
        std::snprintf(percent_text, sizeof(percent_text), "%d%%",
                      state.battery.percent);
    }
    canvas.draw_text(550, 255, percent_text, 5, GrayLevel::Black);

    const char *charging_text = "Charging: N/A";
    if (state.battery.charging_valid) {
        charging_text = state.battery.charging
                            ? "Charging: Yes"
                            : "Charging: No";
    }
    canvas.draw_text(270, 390, charging_text, 3, GrayLevel::Black);
}
