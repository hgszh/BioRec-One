#ifndef SETTINGS_2_PAGE_H
#define SETTINGS_2_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
void settings2_page_init(lv_obj_t *settings2_page);
void settings2_page_destroy(lv_obj_t *settings2_page);
void settings2_page_didAppear(lv_obj_t *settings2_page);
void settings2_page_willDisappear(lv_obj_t *settings2_page);
bool settings2_page_get_autostart_setting(void);
void settings2_page_set_autostart_setting(bool autostart_setting);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif