#include "about_page.h"
#include "pm.h"
#include "ui_font_chinese.h"
#include "ui_hal.h"
#include "ui_img_author_avatar_png.h"

LV_IMG_DECLARE(ui_img_author_avatar_png);
LV_FONT_DECLARE(ui_font_chinese);

static lv_obj_t *ui_image_author_avatar = NULL;
static lv_obj_t *ui_label_author_name = NULL;
static lv_obj_t *ui_label_bilibili = NULL;
static lv_obj_t *ui_label_bilibili_id = NULL;
static lv_obj_t *ui_label_github = NULL;
static lv_obj_t *ui_label_mail = NULL;

/*****************************************************************************************************************/
static void indev_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        back_home_long_beep();
        lv_pm_open_options_t options = {.animation = LV_PM_ANIMA_NONE, .target = LV_PM_TARGET_SELF};
        lv_pm_open_page(0, &options);
    }
}

/*****************************************************************************************************************/
void about_page_init(lv_obj_t *about_page) {
    vTaskDelay(100);
    create_vitrual_btn(about_page, indev_cb);
    lv_obj_set_style_bg_color(about_page, lv_color_hex(0x080808), 0);
    lv_obj_clear_flag(about_page, LV_OBJ_FLAG_SCROLLABLE);

    ui_image_author_avatar = lv_img_create(about_page);
    lv_img_set_src(ui_image_author_avatar, &ui_img_author_avatar_png);
    lv_obj_set_pos(ui_image_author_avatar, -91, 0);
    lv_obj_set_align(ui_image_author_avatar, LV_ALIGN_CENTER);
    lv_obj_set_style_outline_color(ui_image_author_avatar, lv_color_hex(0xF0E68C), 0);
    lv_obj_set_style_outline_width(ui_image_author_avatar, 3, 0);

    ui_label_author_name = lv_label_create(about_page);
    lv_obj_set_pos(ui_label_author_name, 19, -35);
    lv_obj_set_align(ui_label_author_name, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_author_name, "ChirpyJay");
    lv_obj_set_style_text_color(ui_label_author_name, lv_color_hex(0xE6E6E6), 0);
    lv_obj_set_style_text_font(ui_label_author_name, &lv_font_montserrat_16, 0);

    ui_label_bilibili = lv_label_create(about_page);
    lv_obj_set_pos(ui_label_bilibili, 6, -1);
    lv_obj_set_align(ui_label_bilibili, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_bilibili, "Bilibili :");
    lv_obj_set_style_text_color(ui_label_bilibili, lv_color_hex(0xE6E6E6), 0);

    ui_label_bilibili_id = lv_label_create(about_page);
    lv_obj_set_pos(ui_label_bilibili_id, 86, -1);
    lv_obj_set_align(ui_label_bilibili_id, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_bilibili_id, "@学飞小山雀");
    lv_obj_set_style_text_color(ui_label_bilibili_id, lv_color_hex(0xE6E6E6), 0);
    lv_obj_set_style_text_font(ui_label_bilibili_id, &ui_font_chinese, 0);

    ui_label_github = lv_label_create(about_page);
    lv_obj_set_pos(ui_label_github, 41, 20);
    lv_obj_set_align(ui_label_github, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_github, "github.com/hgszh");
    lv_obj_set_style_text_color(ui_label_github, lv_color_hex(0xE6E6E6), 0);

    ui_label_mail = lv_label_create(about_page);
    lv_obj_set_pos(ui_label_mail, 50, 42);
    lv_obj_set_align(ui_label_mail, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_mail, "hgszh@outlook.com");
    lv_obj_set_style_text_color(ui_label_mail, lv_color_hex(0xE6E6E6), 0);
}

void about_page_destroy(lv_obj_t *about_page) {
    (void)about_page;
    ui_image_author_avatar = NULL;
    ui_label_author_name = NULL;
    ui_label_bilibili = NULL;
    ui_label_bilibili_id = NULL;
    ui_label_github = NULL;
    ui_label_mail = NULL;
    destroy_vitrual_btn();
}
