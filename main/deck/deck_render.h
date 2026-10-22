#pragma once

#include <cstdint>

class Canvas;
struct DeckPage;

/** Live status shown in the header (battery %, BLE + Wi-Fi state). */
struct DeckStatus {
    int battery = -1; /* percent 0-100, -1 = unknown */
    bool ble = false; /* BLE host connected */
    bool wifi = false; /* Wi-Fi config AP running */
    bool error = false; /* config failed to parse; using default */
};

/** Draw a page's button grid (monochrome) plus the status header. */
void deck_render_page(Canvas &canvas, const DeckPage &page,
                      const DeckStatus *status);

/** Redraw only the status region of the header (for partial refresh). */
void deck_render_status(Canvas &canvas, const DeckStatus &status);

/** True if (x, y) is in the header bar (tapping it toggles Wi-Fi). */
bool deck_header_hit(uint16_t x, uint16_t y);

/**
 * Hit-test a physical (x, y) coordinate against the page grid.
 * Returns the button index (0-based) or -1 if no button was hit.
 */
int deck_hit_test(const DeckPage &page, uint16_t x, uint16_t y);

/** Redraw one button cell, optionally inverted (press feedback). */
void deck_render_button(Canvas &canvas, const DeckPage &page, int index,
                        bool inverted);
