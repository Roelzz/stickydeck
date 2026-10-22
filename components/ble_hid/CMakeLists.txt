idf_component_register(
    SRCS "ble_hid.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_hid bt nvs_flash
)
