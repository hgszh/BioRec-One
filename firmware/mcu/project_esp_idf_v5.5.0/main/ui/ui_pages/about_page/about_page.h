#ifndef ABOUT_PAGE_H
#define ABOUT_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
void about_page_init(lv_obj_t *about_page);
void about_page_destroy(lv_obj_t *about_page);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif