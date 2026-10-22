#pragma once

#include "esp_err.h"

/**
 * Start a Wi-Fi access point ("StickyDeck-Config", open) and a web server at
 * http://192.168.4.1/ for editing the deck configuration from a browser.
 * Non-fatal: the deck keeps working if this fails.
 */
esp_err_t deck_webserver_start();

/** Set the AP password (must be called before deck_webserver_start). */
void deck_webserver_set_ap_password(const char *password);

/** Stop the web server and Wi-Fi AP (frees power). */
esp_err_t deck_webserver_stop();

/** Toggle the web server / Wi-Fi AP on or off. */
esp_err_t deck_webserver_toggle();

/** Whether the web server / Wi-Fi AP is currently running. */
bool deck_webserver_is_running();
