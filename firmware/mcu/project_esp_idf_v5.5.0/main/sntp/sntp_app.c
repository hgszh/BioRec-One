#include "sntp_app.h"
#include "esp_log.h"
#include "nvs_app.h"
#include "rx8025t.h"
#include "ui_hal.h"
#include <time.h>

static const char *TAG = "sntp";

static void time_sync_notification_cb(struct timeval *tv) {
    settimeofday(tv, NULL);
    ntp_sync_beep();
    // ESP_LOGI(TAG, "Notification of a time synchronization event, sec=%lld", (long long int)tv->tv_sec);
    // sntp_print_time_now();
    sntp_sync_to_rx8025t();
}

void apply_timezone(const char *timezone) {
    setenv("TZ", timezone, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone has been set to: %s", timezone);
}

void load_and_apply_timezone(void) {
    char timezone_buffer[TZ_STRING_MAX_LEN + 1] = {0};
    esp_err_t err = load_timezone_from_nvs(timezone_buffer, sizeof(timezone_buffer));
    if (err == ESP_OK) {
        apply_timezone(timezone_buffer);
    } else {
        ESP_LOGW(TAG, "Error loading timezone from NVS (%s). Using fallback 'CST-8'.", esp_err_to_name(err));
        apply_timezone("CST-8");
        save_timezone_to_nvs("CST-8");
    }
}

void init_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.tencent.com");
    esp_sntp_setservername(1, "ntp.aliyun.com");
    esp_sntp_setservername(2, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}

void sntp_get_timeinfo(struct tm *out_timeinfo) {
    time_t now;
    time(&now);
    localtime_r(&now, out_timeinfo);
}

void sntp_print_time_now(void) {
    struct tm timeinfo;
    sntp_get_timeinfo(&timeinfo);
    if (timeinfo.tm_year + 1900 < 2025) {
        ESP_LOGW(TAG, "Waiting for time sync...");
    } else {
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
    }
}

void sntp_sync_to_rx8025t(void) {
    struct tm timeinfo;
    sntp_get_timeinfo(&timeinfo);
    if (rx8025t_set_time(&timeinfo) != ESP_OK) {
        ESP_LOGW("rx8025t", "Failed to set time");
    }
}

void rx8025t_sync_to_system(void) {
    struct tm timeinfo;
    if (rx8025t_get_time(&timeinfo) == ESP_OK) {
        time_t t = mktime(&timeinfo);
        struct timeval now = {.tv_sec = t};
        settimeofday(&now, NULL);
    } else {
        ESP_LOGW("rx8025t", "Failed to read time");
    }
}
