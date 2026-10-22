#pragma once

#include "app_event.h"
#include "app_state.h"

class Canvas;

// Draws the selected page into the supplied canvas. It does not refresh hardware.
void render_current_page(Canvas &canvas, const AppState &state);

// Refreshes the selected page with its assigned display mode.
// Home uses four-level grayscale; all other pages use monochrome.
esp_err_t refresh_current_page(const AppState &state);

// Restores the page and its displayed data after an AI-button deep-sleep wake.
bool restore_app_state_after_deep_sleep(AppState &state);

void select_previous_page(AppState &state);
void select_next_page(AppState &state);

// Handles one queued event. User actions refresh the page and beep; background
// clock events update only the status-bar time without feedback.
esp_err_t handle_app_event(Canvas &canvas,
                           AppState &state,
                           AppEvent event);
