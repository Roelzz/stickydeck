#pragma once

#include "esp_err.h"

/**
 * Start a Wi-Fi access point ("StickyDeck-Config", open) and a web server at
 * http://192.168.4.1/ for editing the deck configuration from a browser.
 * Non-fatal: the deck keeps working if this fails.
 */
esp_err_t deck_webserver_start();
