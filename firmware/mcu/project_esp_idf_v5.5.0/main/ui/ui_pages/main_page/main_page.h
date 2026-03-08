#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void main_page_init(lv_obj_t *main_page);
void main_page_destroy(lv_obj_t *main_page);
void main_page_didAppear(lv_obj_t *main_page);
void main_page_willDisappear(lv_obj_t *main_page);
typedef enum {
    ECG_SPEED_12_5_MM_S = 0, // 12.5mm/s
    ECG_SPEED_25_MM_S = 1,   // 25mm/s
    ECG_SPEED_50_MM_S = 2,   // 50mm/s
    ECG_SPEED_COUNT
} ecg_paper_speed_t;
void set_ecg_paper_speed(ecg_paper_speed_t speed);
ecg_paper_speed_t get_ecg_paper_speed(void);
uint16_t get_ecg_chart_point_count(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
