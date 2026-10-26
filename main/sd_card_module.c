#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sd_card_module.h"

#define MOUNT_POINT "/sdcard"
#define LOG_FILE    MOUNT_POINT "/log.csv"
#define PIN_MOSI 23
#define PIN_MISO 19
#define PIN_CLK  18
#define PIN_CS   5

static const char *TAG = "SD";
static SemaphoreHandle_t s_mutex = NULL;
static sdmmc_card_t *s_card = NULL;

esp_err_t sd_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    esp_vfs_fat_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = PIN_CS;
    slot.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot, &mount_cfg, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Mount failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SD card mounted at %s", MOUNT_POINT);
    return ESP_OK;
}

esp_err_t sd_log(const char *type, const char *msg)
{
    if (!s_mutex || !s_card) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;

    esp_err_t ret = ESP_OK;
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) {
        ret = ESP_FAIL;
    } else {
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        char ts[24];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &t);
        fprintf(f, "%s,%s,%s\n", ts, type, msg);
        fclose(f);  // closing flushes, so no data is lost on reset
    }
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t sd_read_logs(int last_n)
{
    if (!s_mutex || !s_card) return ESP_ERR_INVALID_STATE;
    if (last_n < 1) last_n = 1;
    if (last_n > 20) last_n = 20;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;

    FILE *f = fopen(LOG_FILE, "r");
    if (!f) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    static char ring[20][128];
    int count = 0;
    while (fgets(ring[count % 20], sizeof(ring[0]), f)) count++;
    fclose(f);
    xSemaphoreGive(s_mutex);

    int total = count < last_n ? count : last_n;
    for (int i = count - total; i < count; i++) printf("%s", ring[i % 20]);
    return ESP_OK;
}