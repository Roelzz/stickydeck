idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES ble_hid esp_driver_gpio
)
