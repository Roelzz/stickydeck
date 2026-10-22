#include "imu_page.h"

#include "app_state.h"
#include "canvas.h"
#include "page.h"

namespace {

constexpr int kCenterX = 400;
constexpr int kCenterY = 290;
constexpr int kShaftHalfWidth = 12;
constexpr int kShaftLength = 90;
constexpr int kHeadLength = 55;

void draw_vertical_arrow(Canvas &canvas, bool points_down)
{
    if (points_down) {
        canvas.fill_rect(kCenterX - kShaftHalfWidth,
                         kCenterY - kShaftLength,
                         kShaftHalfWidth * 2,
                         kShaftLength,
                         GrayLevel::Black);
        for (int row = 0; row < kHeadLength; ++row) {
            const int half_width = kHeadLength - row;
            canvas.fill_rect(kCenterX - half_width,
                             kCenterY + row,
                             half_width * 2 + 1,
                             1,
                             GrayLevel::Black);
        }
    } else {
        canvas.fill_rect(kCenterX - kShaftHalfWidth,
                         kCenterY,
                         kShaftHalfWidth * 2,
                         kShaftLength,
                         GrayLevel::Black);
        for (int row = 0; row < kHeadLength; ++row) {
            const int half_width = row + 1;
            canvas.fill_rect(kCenterX - half_width,
                             kCenterY - kHeadLength + row,
                             half_width * 2 + 1,
                             1,
                             GrayLevel::Black);
        }
    }
}

void draw_horizontal_arrow(Canvas &canvas, bool points_right)
{
    if (points_right) {
        canvas.fill_rect(kCenterX - kShaftLength,
                         kCenterY - kShaftHalfWidth,
                         kShaftLength,
                         kShaftHalfWidth * 2,
                         GrayLevel::Black);
        for (int column = 0; column < kHeadLength; ++column) {
            const int half_height = kHeadLength - column;
            canvas.fill_rect(kCenterX + column,
                             kCenterY - half_height,
                             1,
                             half_height * 2 + 1,
                             GrayLevel::Black);
        }
    } else {
        canvas.fill_rect(kCenterX,
                         kCenterY - kShaftHalfWidth,
                         kShaftLength,
                         kShaftHalfWidth * 2,
                         GrayLevel::Black);
        for (int column = 0; column < kHeadLength; ++column) {
            const int half_height = column + 1;
            canvas.fill_rect(kCenterX - kHeadLength + column,
                             kCenterY - half_height,
                             1,
                             half_height * 2 + 1,
                             GrayLevel::Black);
        }
    }
}

}  // namespace

void imu_page_render(Canvas &canvas, const AppState &state)
{
    page_draw_layout(canvas, state, "Hello Sticky", "IMU Page");

    if (!state.imu.valid) {
        canvas.draw_text(238, 280, "IMU data unavailable",
                         3, GrayLevel::Black);
        return;
    }

    canvas.draw_text(148, 405,
                     "Rotate the device. The arrow points down.",
                     2, GrayLevel::Black);

    switch (state.imu.orientation) {
    case StickyImuOrientation::ArrowUp:
        draw_vertical_arrow(canvas, false);
        break;
    case StickyImuOrientation::ArrowDown:
        draw_vertical_arrow(canvas, true);
        break;
    case StickyImuOrientation::ArrowLeft:
        draw_horizontal_arrow(canvas, false);
        break;
    case StickyImuOrientation::ArrowRight:
        draw_horizontal_arrow(canvas, true);
        break;
    case StickyImuOrientation::Unknown:
    case StickyImuOrientation::FaceUp:
    case StickyImuOrientation::FaceDown:
        canvas.draw_text(220, 280, "Place device upright",
                         3, GrayLevel::Black);
        break;
    }
}
