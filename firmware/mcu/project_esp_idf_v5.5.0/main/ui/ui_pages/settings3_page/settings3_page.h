#ifndef SETTINGS_3_PAGE_H
#define SETTINGS_3_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
void settings3_page_init(lv_obj_t *settings3_page);
void settings3_page_destroy(lv_obj_t *settings3_page);
void settings3_page_willDisappear(lv_obj_t *settings3_page);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif