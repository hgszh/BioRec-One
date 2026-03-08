#ifndef __SNTP_APP_H__
#define __SNTP_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_sntp.h"
#include <time.h>
#define TZ_STRING_MAX_LEN 128

void init_sntp(void);
void sntp_print_time_now(void);
void sntp_get_timeinfo(struct tm *out_timeinfo);
void apply_timezone(const char *timezone);
void load_and_apply_timezone(void);

void sntp_sync_to_rx8025t(void);
void rx8025t_sync_to_system(void);

#ifdef __cplusplus
}
#endif

#endif