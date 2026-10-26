idf_component_register(
    SRCS "main.c" "sd_card_module.c" "schedule_module.c"
    INCLUDE_DIRS "."
    REQUIRES fatfs sdmmc driver nvs_flash
)