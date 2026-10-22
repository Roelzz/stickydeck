#include "home_page.h"

#include "app_state.h"
#include "canvas.h"
#include "page.h"

void home_page_render(Canvas &canvas, const AppState &state)
{
    page_draw_layout(canvas, state, "Hello Sticky", "Home Page");

    constexpr int bar_x = 72;
    constexpr int bar_y = 250;
    constexpr int bar_width = 158;
    constexpr int bar_height = 112;
    constexpr int bar_gap = 6;
    constexpr GrayLevel shades[] = {
        GrayLevel::Black,
        GrayLevel::DarkGray,
        GrayLevel::LightGray,
        GrayLevel::White,
    };

    for (int index = 0; index < 4; ++index) {
        const int x = bar_x + index * (bar_width + bar_gap);
        canvas.fill_rect(x, bar_y, bar_width, bar_height, shades[index]);
        canvas.draw_rect(x, bar_y, bar_width, bar_height, GrayLevel::Black);
    }

    canvas.draw_text(72, 390, "GRAY 0  1  2  3", 2, GrayLevel::Black);
}
