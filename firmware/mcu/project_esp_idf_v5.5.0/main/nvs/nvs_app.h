#ifndef _NVS_APP_H
#define _NVS_APP_H

#include "esp_err.h"
#include <stdbool.h>
void init_nvs(void);
void nvs_save_config(void);
void save_timezone_to_nvs(const char *timezone);
esp_err_t load_timezone_from_nvs(char *buffer, size_t max_len);

#endif
