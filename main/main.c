#include "esp_log.h"
#include "sd_card_module.h"
#include "schedule_module.h"

void app_main(void)
{
    if (sd_init() == ESP_OK) {
        sd_log("TEST", "hello from member 4");
        sd_read_logs(5);
    }
}