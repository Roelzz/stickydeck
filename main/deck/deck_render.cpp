#include "deck_render.h"

#include <cstdio>
#include <cstring>

#include "canvas.h"
#include "deck_config.h"
#include "font.h"
#include "sticky_display.h"

namespace {

constexpr uint16_t kHeaderHeight = 44;
constexpr uint16_t kGutter = 6;

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

}  // namespace

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
    const uint8_t scale = fit_scale(label, g.w - 10, g.h - 8);
    const int text_w = static_cast<int>(strlen(label)) *
                       (kFontWidth + kFontSpacing) * scale;
    const int text_h = kFontHeight * scale;
    const int tx = g.x + (g.w - text_w) / 2;
    const int ty = g.y + (g.h - text_h) / 2;
    canvas.draw_text(tx, ty, label, scale, line);
}

void deck_render_page(Canvas &canvas, const DeckPage &page)
{
    canvas.clear(GrayLevel::White);

    /* Header: title + page id. */
    char title[64] = {};
    std::snprintf(title, sizeof(title), "%s", page.title.c_str());
    canvas.draw_text(12, 12, title, 2, GrayLevel::Black);

    const int cols = page.cols > 0 ? page.cols : 1;
    const int rows = page.rows > 0 ? page.rows : 1;
    char indicator[32] = {};
    std::snprintf(indicator, sizeof(indicator), "%dx%d", cols, rows);
    const int indicator_w = static_cast<int>(strlen(indicator)) *
                            (kFontWidth + kFontSpacing) * 2;
    canvas.draw_text(kStickyDisplayWidth - indicator_w - 12, 12, indicator, 2,
                     GrayLevel::Black);

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
