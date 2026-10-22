#include "note_page.h"

#include <cstddef>

#include "app_state.h"
#include "canvas.h"
#include "page.h"

namespace {

constexpr size_t kCharactersPerLine = 52;
constexpr int kMaximumLines = 6;
constexpr int kTextX = 76;
constexpr int kTextY = 240;
constexpr int kLineSpacing = 32;

void draw_wrapped_text(Canvas &canvas, const char *text)
{
    char line[kCharactersPerLine + 1U] = {};
    size_t line_length = 0;
    int line_number = 0;

    for (const char *cursor = text;
         *cursor != '\0' && line_number < kMaximumLines;
         ++cursor) {
        if (*cursor == '\r') {
            continue;
        }

        const bool line_break = *cursor == '\n';
        if (!line_break && line_length < kCharactersPerLine) {
            line[line_length++] = *cursor == '\t' ? ' ' : *cursor;
        }

        if (line_break || line_length == kCharactersPerLine) {
            line[line_length] = '\0';
            canvas.draw_text(kTextX,
                             kTextY + line_number * kLineSpacing,
                             line,
                             2,
                             GrayLevel::Black);
            ++line_number;
            line_length = 0;
        }
    }

    if (line_length > 0 && line_number < kMaximumLines) {
        line[line_length] = '\0';
        canvas.draw_text(kTextX,
                         kTextY + line_number * kLineSpacing,
                         line,
                         2,
                         GrayLevel::Black);
    }
}

}  // namespace

void note_page_render(Canvas &canvas, const AppState &state)
{
    page_draw_layout(canvas, state, "Hello Sticky", "Note Page");

    if (state.note.valid) {
        draw_wrapped_text(canvas, state.note.text);
    } else {
        canvas.draw_text(kTextX, kTextY, "MicroSD card not detected.",
                         2, GrayLevel::Black);
        canvas.draw_text(kTextX, kTextY + 40, "Check the MicroSD card.",
                         2, GrayLevel::Black);
    }
}
