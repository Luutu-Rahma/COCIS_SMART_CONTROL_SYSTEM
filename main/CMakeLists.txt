idf_component_register(
    SRCS "main.c" "sd_card_module.c" "schedule_module.c"
    INCLUDE_DIRS "."
    REQUIRES fatfs sdmmc esp_driver_sdspi esp_driver_spi esp_driver_gpio nvs_flash
)