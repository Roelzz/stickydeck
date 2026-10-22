#include "deck_render.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "canvas.h"
#include "deck_config.h"
#include "font.h"
#include "sticky_display.h"

namespace {

constexpr uint16_t kHeaderHeight = 44;
constexpr uint16_t kGutter = 6;

/* Right-hand status region in the header. */
constexpr int kStatusX = 480;
constexpr int kStatusY = 0;
constexpr int kStatusW = 800 - kStatusX;
constexpr int kStatusH = kHeaderHeight;

struct CellGeometry {
    int x;
    int y;
    int w;
    int h;
};

CellGeometry cell_geometry(const DeckPage &page, int index)
{
    const int cols = page.cols > 0 ? page.cols : 1;
    const int rows = page.rows > 0 ? page.rows : 1;

    const int cell_w = kStickyDisplayWidth / cols;
    const int cell_h = (kStickyDisplayHeight - kHeaderHeight) / rows;

    const int col = index % cols;
    const int row = index / cols;

    CellGeometry g;
    g.x = col * cell_w + kGutter / 2;
    g.y = kHeaderHeight + row * cell_h + kGutter / 2;
    g.w = cell_w - kGutter;
    g.h = cell_h - kGutter;
    return g;
}

uint8_t fit_scale(const char *text, int max_width, int max_height)
{
    uint8_t scale = 3;
    while (scale > 1) {
        const int text_w = static_cast<int>(strlen(text)) *
                               (kFontWidth + kFontSpacing) * scale;
        const int text_h = kFontHeight * scale;
        if (text_w <= max_width && text_h <= max_height) {
            break;
        }
        --scale;
    }
    return scale;
}

int text_width(const char *text, uint8_t scale)
{
    return static_cast<int>(strlen(text)) * (kFontWidth + kFontSpacing) * scale;
}

/* Draw a simple built-in icon centered at (cx, cy); `size` is half-extent. */
void draw_icon(Canvas &canvas, int cx, int cy, int size, const char *icon,
               GrayLevel color)
{
    if (icon == nullptr || icon[0] == '\0') {
        return;
    }
    const int s = size;

    if (strcmp(icon, "arrow") == 0) {
        canvas.draw_line(cx - s, cy, cx + s / 2, cy, color);
        canvas.draw_line(cx + s / 2, cy, cx, cy - s / 2, color);
        canvas.draw_line(cx + s / 2, cy, cx, cy + s / 2, color);
    } else if (strcmp(icon, "circle") == 0) {
        canvas.draw_circle(cx, cy, s, color);
    } else if (strcmp(icon, "whiteboard") == 0) {
        canvas.draw_rect(cx - s, cy - s * 3 / 4, s * 2, s * 3 / 2, color);
        canvas.draw_line(cx - s * 3 / 4, cy, cx + s * 3 / 4, cy, color);
    } else if (strcmp(icon, "text") == 0) {
        canvas.draw_line(cx - s, cy - s, cx + s, cy - s, color);
        canvas.draw_line(cx, cy - s, cx, cy + s, color);
    } else if (strcmp(icon, "zoom") == 0) {
        canvas.draw_circle(cx - s / 4, cy - s / 4, s * 3 / 5, color);
        canvas.draw_line(cx + s / 4, cy + s / 4, cx + s, cy + s, color);
    } else if (strcmp(icon, "annotate") == 0) {
        canvas.draw_line(cx - s, cy + s, cx + s * 3 / 5, cy - s * 3 / 5, color);
        canvas.draw_line(cx + s * 3 / 5, cy - s * 3 / 5, cx + s, cy - s, color);
        canvas.draw_line(cx + s * 3 / 5, cy - s * 3 / 5, cx + s,
                         cy - s * 3 / 5, color);
    }
}

}  // namespace

void draw_indicator(Canvas &canvas, int x, int y, bool on)
{
    if (on) {
        canvas.fill_rect(x, y, 8, 8, GrayLevel::Black);
    } else {
        canvas.draw_rect(x, y, 8, 8, GrayLevel::Black);
    }
}

void deck_render_status(Canvas &canvas, const DeckStatus &status)
{
    /* Clear the status region so stale content doesn't linger. */
    canvas.fill_rect(kStatusX, kStatusY, kStatusW, kStatusH, GrayLevel::White);

    const int square_y = (kStatusH - 8) / 2;
    const int label_y = (kStatusH - kFontHeight) / 2;
    int x = kStatusX + 4;

    /* BLE indicator. */
    canvas.draw_text(x, label_y, "BLE", 1, GrayLevel::Black);
    x += 3 * (kFontWidth + kFontSpacing) + 2;
    draw_indicator(canvas, x, square_y, status.ble);
    x += 8 + 14;

    /* Wi-Fi indicator (tap the header to toggle). */
    canvas.draw_text(x, label_y, "WiFi", 1, GrayLevel::Black);
    x += 4 * (kFontWidth + kFontSpacing) + 2;
    draw_indicator(canvas, x, square_y, status.wifi);

    /* Battery percentage, right-aligned. */
    char battery[8] = "--";
    if (status.battery >= 0 && status.battery <= 100) {
        std::snprintf(battery, sizeof(battery), "%d%%", status.battery);
    }
    const uint8_t scale = 2;
    const int bx = kStickyDisplayWidth - 8 - text_width(battery, scale);
    canvas.draw_text(bx, (kStatusH - kFontHeight * scale) / 2, battery, scale,
                     GrayLevel::Black);
}

bool deck_header_hit(uint16_t x, uint16_t y)
{
    (void)x;
    return y < kHeaderHeight;
}

void deck_render_button(Canvas &canvas, const DeckPage &page, int index,
                        bool inverted)
{
    if (index < 0 || index >= static_cast<int>(page.buttons.size())) {
        return;
    }

    const CellGeometry g = cell_geometry(page, index);
    const GrayLevel fill = inverted ? GrayLevel::Black : GrayLevel::White;
    const GrayLevel line = inverted ? GrayLevel::White : GrayLevel::Black;

    canvas.fill_rect(g.x, g.y, g.w, g.h, fill);
    canvas.draw_rect(g.x, g.y, g.w, g.h, line);

    const char *label = page.buttons[static_cast<size_t>(index)].label.c_str();
    const char *icon = page.buttons[static_cast<size_t>(index)].icon.c_str();

    if (icon[0] != '\0') {
        /* Icon centered in the upper area, small label below. */
        const int icon_size = std::min(g.w, g.h) * 22 / 100;
        const int icon_cy = g.y + g.h * 35 / 100;
        draw_icon(canvas, g.x + g.w / 2, icon_cy, icon_size, icon, line);

        const uint8_t scale = fit_scale(label, g.w - 8, g.h * 30 / 100);
        const int label_w = text_width(label, scale);
        const int label_h = kFontHeight * scale;
        const int tx = g.x + (g.w - label_w) / 2;
        const int ty = g.y + g.h * 70 / 100 - label_h / 2;
        canvas.draw_text(tx, ty, label, scale, line);
    } else {
        /* Label only, centered. */
        const uint8_t scale = fit_scale(label, g.w - 10, g.h - 8);
        const int label_w = text_width(label, scale);
        const int label_h = kFontHeight * scale;
        const int tx = g.x + (g.w - label_w) / 2;
        const int ty = g.y + (g.h - label_h) / 2;
        canvas.draw_text(tx, ty, label, scale, line);
    }
}

void deck_render_page(Canvas &canvas, const DeckPage &page,
                      const DeckStatus *status)
{
    canvas.clear(GrayLevel::White);

    /* Header: title (left) + status (right). */
    canvas.draw_text(12, 12, page.title.c_str(), 2, GrayLevel::Black);
    if (status != nullptr) {
        deck_render_status(canvas, *status);
    }

    canvas.draw_line(0, kHeaderHeight - 2, kStickyDisplayWidth,
                     kHeaderHeight - 2, GrayLevel::Black);

    for (size_t i = 0; i < page.buttons.size(); ++i) {
        deck_render_button(canvas, page, static_cast<int>(i), false);
    }
}

int deck_hit_test(const DeckPage &page, uint16_t x, uint16_t y)
{
    if (y < kHeaderHeight) {
        return -1;
    }

    const int cols = page.cols > 0 ? page.cols : 1;
    const int rows = page.rows > 0 ? page.rows : 1;

    const int cell_w = kStickyDisplayWidth / cols;
    const int cell_h = (kStickyDisplayHeight - kHeaderHeight) / rows;

    const int col = x / cell_w;
    const int row = (y - kHeaderHeight) / cell_h;
    if (col < 0 || col >= cols || row < 0 || row >= rows) {
        return -1;
    }

    const int index = row * cols + col;
    if (index >= static_cast<int>(page.buttons.size())) {
        return -1;
    }
    return index;
}
