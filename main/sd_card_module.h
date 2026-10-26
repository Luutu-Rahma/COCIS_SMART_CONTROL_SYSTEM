#pragma once
#include "esp_err.h"

esp_err_t sd_init(void);
esp_err_t sd_log(const char *type, const char *msg);  // appends "timestamp,type,msg"
esp_err_t sd_read_logs(int last_n);                   // prints last N lines (max 20)