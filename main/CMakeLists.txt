idf_component_register(
    SRCS
        "main.cpp"
        "app/app_event.cpp"
        "board/board.cpp"

        "devices/sticky_display.cpp"
        "devices/sticky_buzzer.cpp"
        "devices/sticky_buttons.cpp"
        "devices/sticky_touch.cpp"
        "devices/sticky_sdcard.cpp"
        "devices/sticky_power.cpp"
        "devices/sticky_battery.cpp"

        "ui/canvas.cpp"
        "ui/font.cpp"

        "deck/deck_app.cpp"
        "deck/deck_config.cpp"
        "deck/deck_actions.cpp"
        "deck/deck_render.cpp"
    INCLUDE_DIRS "." "app" "board" "devices" "deck" "ui"
    REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_ledc
        esp_driver_spi
        esp_hw_support
        esp_timer
        button
        bq27220
        fatfs
        gt911
        sdmmc
        seeed_epaper
        ble_hid
        json
)
