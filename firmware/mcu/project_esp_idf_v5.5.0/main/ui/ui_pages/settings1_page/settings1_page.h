#ifndef SETTINGS_1_PAGE_H
#define SETTINGS_1_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void settings1_page_init(lv_obj_t *settings1_page);
void settings1_page_destroy(lv_obj_t *settings1_page);
void settings1_page_willDisappear(lv_obj_t *settings1_page);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
