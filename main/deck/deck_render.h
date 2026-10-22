#pragma once

#include <cstdint>

class Canvas;
struct DeckPage;

/** Draw a page's button grid (monochrome) into the canvas. */
void deck_render_page(Canvas &canvas, const DeckPage &page);

/**
 * Hit-test a physical (x, y) coordinate against the page grid.
 * Returns the button index (0-based) or -1 if no button was hit.
 */
int deck_hit_test(const DeckPage &page, uint16_t x, uint16_t y);

/** Redraw one button cell, optionally inverted (press feedback). */
void deck_render_button(Canvas &canvas, const DeckPage &page, int index,
                        bool inverted);
