#include "settings3_page.h"
#include "data_logger.h"
#include "nvs_app.h"
#include "pm.h"
#include "ui_hal.h"
#include "ui_img_icon_down_png.h"
#include "ui_img_icon_up_png.h"

LV_IMG_DECLARE(ui_img_icon_rectangle2_png);
LV_IMG_DECLARE(ui_img_icon_up_png);
LV_IMG_DECLARE(ui_img_icon_down_png);
LV_FONT_DECLARE(ui_font_agencyb17);

static lv_obj_t *ui_label_recording_duration = NULL;
static lv_obj_t *ui_pannel_hours = NULL;
static lv_obj_t *ui_label_hours_value = NULL;
static lv_obj_t *ui_label_hours = NULL;
static lv_obj_t *ui_label_minutes = NULL;
static lv_obj_t *ui_button_hours_up = NULL;
static lv_obj_t *ui_image_hours_up = NULL;
static lv_obj_t *ui_button_hours_down = NULL;
static lv_obj_t *ui_image_hours_down = NULL;
static lv_obj_t *ui_pannel_minutes = NULL;
static lv_obj_t *ui_label_minutes_value = NULL;
static lv_obj_t *ui_button_minutes_up = NULL;
static lv_obj_t *ui_image_minutes_up = NULL;
static lv_obj_t *ui_button_minutes_down = NULL;
static lv_obj_t *ui_image_minutes_down = NULL;
static lv_obj_t *ui_label_recording_countdown = NULL;
static lv_obj_t *ui_pannel_seconds = NULL;
static lv_obj_t *ui_label_seconds_value = NULL;
static lv_obj_t *ui_label_seconds = NULL;
static lv_obj_t *ui_button_seconds_up = NULL;
static lv_obj_t *ui_image_seconds_up = NULL;
static lv_obj_t *ui_button_seconds_down = NULL;
static lv_obj_t *ui_image_seconds_down = NULL;
static lv_obj_t *ui_label_file_split_threshold = NULL;
static lv_obj_t *ui_pannel_mb = NULL;
static lv_obj_t *ui_label_mb_value = NULL;
static lv_obj_t *ui_label_mb = NULL;
static lv_obj_t *ui_button_mb_up = NULL;
static lv_obj_t *ui_image_mb_up = NULL;
static lv_obj_t *ui_button_mb_down = NULL;
static lv_obj_t *ui_image_mb_down = NULL;
static lv_obj_t *ui_label_file_format = NULL;
static lv_obj_t *ui_checkbox_csv = NULL;
static lv_obj_t *ui_checkbox_bdf = NULL;
static lv_obj_t *ui_divider_line0 = NULL;
static lv_obj_t *ui_divider_line1 = NULL;
static lv_obj_t *ui_divider_line2 = NULL;
static lv_group_t *g_settings3_page = NULL;

/*****************************************************************************************************************/
static void indev_cb(lv_event_t *e) {
    uint8_t btn_id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *focused_obj = lv_group_get_focused(g_settings3_page);
    if (code == LV_EVENT_PRESSED) {
        if (lv_obj_check_type(focused_obj, &lv_btn_class) && btn_id == 3) {
            lv_obj_add_state(focused_obj, LV_STATE_PRESSED);
        }
    }
    if (code == LV_EVENT_CLICKED) {
        switch (btn_id) {
        case 0: lv_group_focus_prev(g_settings3_page); break;
        case 1: lv_group_focus_next(g_settings3_page); break;
        case 2:
            btn_beep();
            lv_pm_open_options_t options = {.animation = LV_PM_ANIMA_SLIDE_LEFT, .target = LV_PM_TARGET_SELF};
            lv_pm_open_page(0, &options);
            break;
        case 3:
            if (lv_obj_check_type(focused_obj, &lv_checkbox_class)) {
                if (lv_obj_has_state(focused_obj, LV_STATE_CHECKED)) lv_obj_clear_state(focused_obj, LV_STATE_CHECKED);
                else lv_obj_add_state(focused_obj, LV_STATE_CHECKED);
                lv_event_send(focused_obj, LV_EVENT_VALUE_CHANGED, NULL);
            } else if (lv_obj_check_type(focused_obj, &lv_btn_class)) {
                lv_obj_clear_state(focused_obj, LV_STATE_PRESSED);
                lv_event_send(focused_obj, LV_EVENT_CLICKED, NULL);
            }
            break;
        default: break;
        }
        btn_beep();
    }
    if (code == LV_EVENT_LONG_PRESSED_REPEAT) {
        switch (btn_id) {
        case 0:
            lv_group_focus_prev(g_settings3_page);
            btn_beep();
            break;
        case 1:
            lv_group_focus_next(g_settings3_page);
            btn_beep();
            break;
        case 2:
            back_home_long_beep();
            lv_pm_open_options_t options = {.animation = LV_PM_ANIMA_SLIDE_RIGHT, .target = LV_PM_TARGET_SELF};
            lv_pm_open_page(0, &options);
            break;
        case 3:
            if (lv_obj_check_type(focused_obj, &lv_btn_class)) {
                btn_beep();
                lv_event_send(focused_obj, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
            }
            break;
        default: break;
        }
    }
}

/*****************************************************************************************************************/
static void handle_file_format_checkbox(lv_obj_t *active_checkbox) {
    if (active_checkbox != ui_checkbox_csv && active_checkbox != ui_checkbox_bdf) return;
    // 单选组按钮不能通过再次点击来取消
    // 若再次点击已选中的单选框，导致其被“取消选中”，则应将其重新选中
    if (!lv_obj_has_state(active_checkbox, LV_STATE_CHECKED)) {
        lv_obj_add_state(active_checkbox, LV_STATE_CHECKED);
        return;
    }
    // 如果它不是刚刚被点击的那个，则除去选定态
    if (ui_checkbox_csv != active_checkbox) {
        lv_obj_clear_state(ui_checkbox_csv, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_checkbox_bdf, LV_STATE_CHECKED);
    }
    if (lv_obj_has_state(ui_checkbox_csv, LV_STATE_CHECKED)) {
        data_logger_set_format(DATA_FORMAT_CSV);
    } else {
        data_logger_set_format(DATA_FORMAT_BDF);
    }
}

static int32_t recording_duration_minutes = 0;
static void update_duration_value(lv_event_code_t code, lv_obj_t *target_obj) {
    if (target_obj != ui_button_hours_up && target_obj != ui_button_hours_down && target_obj != ui_button_minutes_up &&
        target_obj != ui_button_minutes_down)
        return;
    int step = (code == LV_EVENT_CLICKED) ? 1 : 5;
    // 增加/减少小时数
    if (target_obj == ui_button_hours_up) recording_duration_minutes += step * 60;
    else if (target_obj == ui_button_hours_down) recording_duration_minutes -= step * 60;
    // 增加/减少分钟数
    else if (target_obj == ui_button_minutes_up) recording_duration_minutes += step;
    else if (target_obj == ui_button_minutes_down) recording_duration_minutes -= step;
    // 限制总时长在0到5999分钟（即99小时59分钟）之间
    if (recording_duration_minutes < 0) recording_duration_minutes = 0;
    if (recording_duration_minutes > 5999) recording_duration_minutes = 5999;
    int hours = recording_duration_minutes / 60;
    int minutes = recording_duration_minutes % 60;
    lv_label_set_text_fmt(ui_label_hours_value, "%d", hours);
    lv_label_set_text_fmt(ui_label_minutes_value, "%d", minutes);
    data_logger_set_duration(recording_duration_minutes);
}

const uint8_t recording_countdown_seconds_options[8] = {0, 3, 5, 10, 15, 20, 30, 60};
static int countdown_option_index = 0;
static void update_countdown_value(lv_obj_t *target_obj) {
    if (target_obj != ui_button_seconds_up && target_obj != ui_button_seconds_down) return;
    if (target_obj == ui_button_seconds_up) {
        if (countdown_option_index < 7) countdown_option_index++;
    } else {
        if (countdown_option_index > 0) countdown_option_index--;
    }
    uint8_t new_countdown_value = recording_countdown_seconds_options[countdown_option_index];
    lv_label_set_text_fmt(ui_label_seconds_value, "%u", new_countdown_value);
    data_logger_set_countdown(new_countdown_value);
}

const uint16_t file_split_size_options[7] = {0, 10, 50, 100, 200, 500, 1024};
static int split_size_option_index = 0;
static void update_split_size_value(lv_obj_t *target_obj) {
    if (target_obj != ui_button_mb_up && target_obj != ui_button_mb_down) return;
    if (target_obj == ui_button_mb_up) {
        if (split_size_option_index < 6) split_size_option_index++;
    } else {
        if (split_size_option_index > 0) split_size_option_index--;
    }
    uint16_t new_split_size = file_split_size_options[split_size_option_index];
    lv_label_set_text_fmt(ui_label_mb_value, "%u", new_split_size);
    data_logger_set_split_size(new_split_size);
}

static void settings3_event_cb(lv_event_t *e) {
    lv_obj_t *target_obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        handle_file_format_checkbox(target_obj);
    } else if (code == LV_EVENT_CLICKED || code == LV_EVENT_LONG_PRESSED_REPEAT) {
        update_duration_value(code, target_obj);
        update_countdown_value(target_obj);
        update_split_size_value(target_obj);
    }
}

/*****************************************************************************************************************/
void settings3_page_init(lv_obj_t *settings3_page) {
    static lv_style_t style_item_title;
    lv_style_init(&style_item_title);
    lv_style_set_text_color(&style_item_title, lv_color_hex(0x03CB2B));
    lv_style_set_text_letter_space(&style_item_title, 1);
    lv_style_set_text_font(&style_item_title, &ui_font_agencyb17);
    static lv_style_t style_item_value;
    lv_style_init(&style_item_value);
    lv_style_set_text_color(&style_item_value, lv_color_hex(0x00FF00));
    lv_style_set_text_font(&style_item_value, &lv_font_montserrat_16);
    static lv_style_t style_pannel;
    lv_style_init(&style_pannel);
    lv_style_set_radius(&style_pannel, 2);
    lv_style_set_bg_color(&style_pannel, lv_color_hex(0x383A38));
    lv_style_set_border_color(&style_pannel, lv_color_hex(0x383A38));
    lv_style_set_text_color(&style_pannel, lv_color_hex(0x00FF00));
    static lv_style_t style_button_primary;
    lv_style_init(&style_button_primary);
    lv_style_set_radius(&style_button_primary, 2);
    lv_style_set_bg_color(&style_button_primary, lv_color_hex(0x00AA00));
    lv_style_set_shadow_color(&style_button_primary, lv_color_hex(0x000000));
    lv_style_set_shadow_opa(&style_button_primary, 0);
    static lv_style_t style_button_focused;
    lv_style_init(&style_button_focused);
    lv_style_set_border_width(&style_button_focused, 2);
    lv_style_set_border_color(&style_button_focused, lv_color_hex(0x000000));
    lv_style_set_outline_width(&style_button_focused, 2);
    lv_style_set_outline_color(&style_button_focused, lv_color_hex(0xFF9800));
    static lv_style_t style_label_unit;
    lv_style_init(&style_label_unit);
    lv_style_set_text_color(&style_label_unit, lv_color_hex(0x03CB2B));
    lv_style_set_text_font(&style_label_unit, &lv_font_montserrat_14);
    static lv_style_t style_checkbox_label_value;
    lv_style_init(&style_checkbox_label_value);
    lv_style_set_text_color(&style_checkbox_label_value, lv_color_hex(0x03CB2B));
    lv_style_set_text_letter_space(&style_checkbox_label_value, 1);
    lv_style_set_text_font(&style_checkbox_label_value, &lv_font_montserrat_14);
    static lv_style_t style_checkbox_indicator_default;
    lv_style_init(&style_checkbox_indicator_default);
    lv_style_set_radius(&style_checkbox_indicator_default, 0);
    lv_style_set_bg_color(&style_checkbox_indicator_default, lv_color_hex(0x000000));
    lv_style_set_border_color(&style_checkbox_indicator_default, lv_color_hex(0x03CB2B));
    static lv_style_t style_checkbox_indicator_checked;
    lv_style_init(&style_checkbox_indicator_checked);
    lv_style_set_radius(&style_checkbox_indicator_checked, 0);
    lv_style_set_bg_color(&style_checkbox_indicator_checked, lv_color_hex(0x000000));
    lv_style_set_bg_img_src(&style_checkbox_indicator_checked, &ui_img_icon_rectangle2_png);
    static lv_style_t style_checkbox_focused;
    lv_style_init(&style_checkbox_focused);
    lv_style_set_border_color(&style_checkbox_focused, lv_color_hex(0xFF9800));
    static lv_style_t style_divider_line;
    lv_style_init(&style_divider_line);
    lv_style_set_border_color(&style_divider_line, lv_color_hex(0x000000));
    lv_style_set_border_opa(&style_divider_line, 230);

    create_vitrual_btn(settings3_page, indev_cb);
    lv_obj_set_style_bg_color(settings3_page, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(settings3_page, LV_OBJ_FLAG_SCROLLABLE);
    g_settings3_page = lv_group_create();
    lv_group_set_default(g_settings3_page);
    lv_indev_set_group(indev_button, g_settings3_page);

    ui_label_recording_duration = lv_label_create(settings3_page);
    lv_obj_set_pos(ui_label_recording_duration, -85, -94);
    lv_obj_set_align(ui_label_recording_duration, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_recording_duration, "Recording Duration");
    lv_obj_add_style(ui_label_recording_duration, &style_item_title, 0);

    ui_pannel_hours = lv_obj_create(settings3_page);
    lv_obj_clear_flag(ui_pannel_hours, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui_pannel_hours, 45, 28);
    lv_obj_set_pos(ui_pannel_hours, -83, -61);
    lv_obj_set_align(ui_pannel_hours, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_pannel_hours, &style_pannel, 0);
    ui_label_hours_value = lv_label_create(ui_pannel_hours);
    lv_obj_set_pos(ui_label_hours_value, 1, 0);
    lv_obj_set_align(ui_label_hours_value, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_hours_value, "0");
    lv_obj_add_style(ui_label_hours_value, &style_item_value, 0);

    ui_label_hours = lv_label_create(settings3_page);
    lv_obj_set_pos(ui_label_hours, -82, -38);
    lv_obj_set_align(ui_label_hours, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_hours, "hours");
    lv_obj_add_style(ui_label_hours, &style_label_unit, 0);

    ui_button_hours_up = lv_btn_create(settings3_page);
    lv_obj_set_size(ui_button_hours_up, 20, 20);
    lv_obj_set_pos(ui_button_hours_up, -119, -61);
    lv_obj_set_align(ui_button_hours_up, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_button_hours_up, &style_button_primary, 0);
    lv_obj_add_style(ui_button_hours_up, &style_button_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings3_page, ui_button_hours_up);
    lv_obj_add_event_cb(ui_button_hours_up, settings3_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_button_hours_up, settings3_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    ui_image_hours_up = lv_img_create(ui_button_hours_up);
    lv_img_set_src(ui_image_hours_up, &ui_img_icon_up_png);
    lv_obj_set_align(ui_image_hours_up, LV_ALIGN_CENTER);
    lv_img_set_zoom(ui_image_hours_up, 170);

    ui_button_hours_down = lv_btn_create(settings3_page);
    lv_obj_set_size(ui_button_hours_down, 20, 20);
    lv_obj_set_pos(ui_button_hours_down, -46, -61);
    lv_obj_set_align(ui_button_hours_down, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_button_hours_down, &style_button_primary, 0);
    lv_obj_add_style(ui_button_hours_down, &style_button_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings3_page, ui_button_hours_down);
    lv_obj_add_event_cb(ui_button_hours_down, settings3_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_button_hours_down, settings3_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    ui_image_hours_down = lv_img_create(ui_button_hours_down);
    lv_img_set_src(ui_image_hours_down, &ui_img_icon_down_png);
    lv_obj_set_align(ui_image_hours_down, LV_ALIGN_CENTER);
    lv_img_set_zoom(ui_image_hours_down, 170);

    ui_pannel_minutes = lv_obj_create(settings3_page);
    lv_obj_clear_flag(ui_pannel_minutes, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui_pannel_minutes, 45, 28);
    lv_obj_set_pos(ui_pannel_minutes, 40, -61);
    lv_obj_set_align(ui_pannel_minutes, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_pannel_minutes, &style_pannel, 0);
    ui_label_minutes_value = lv_label_create(ui_pannel_minutes);
    lv_obj_set_pos(ui_label_minutes_value, 1, 0);
    lv_obj_set_align(ui_label_minutes_value, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_minutes_value, "0");
    lv_obj_add_style(ui_label_minutes_value, &style_item_value, 0);

    ui_label_minutes = lv_label_create(settings3_page);
    lv_obj_set_pos(ui_label_minutes, 42, -38);
    lv_obj_set_align(ui_label_minutes, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_minutes, "minutes");
    lv_obj_add_style(ui_label_minutes, &style_label_unit, 0);

    ui_button_minutes_up = lv_btn_create(settings3_page);
    lv_obj_set_size(ui_button_minutes_up, 20, 20);
    lv_obj_set_pos(ui_button_minutes_up, 4, -61);
    lv_obj_set_align(ui_button_minutes_up, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_button_minutes_up, &style_button_primary, 0);
    lv_obj_add_style(ui_button_minutes_up, &style_button_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings3_page, ui_button_minutes_up);
    lv_obj_add_event_cb(ui_button_minutes_up, settings3_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_button_minutes_up, settings3_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    ui_image_minutes_up = lv_img_create(ui_button_minutes_up);
    lv_img_set_src(ui_image_minutes_up, &ui_img_icon_up_png);
    lv_obj_set_align(ui_image_minutes_up, LV_ALIGN_CENTER);
    lv_img_set_zoom(ui_image_minutes_up, 170);

    ui_button_minutes_down = lv_btn_create(settings3_page);
    lv_obj_set_size(ui_button_minutes_down, 20, 20);
    lv_obj_set_pos(ui_button_minutes_down, 77, -61);
    lv_obj_set_align(ui_button_minutes_down, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_button_minutes_down, &style_button_primary, 0);
    lv_obj_add_style(ui_button_minutes_down, &style_button_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings3_page, ui_button_minutes_down);
    lv_obj_add_event_cb(ui_button_minutes_down, settings3_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_button_minutes_down, settings3_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    ui_image_minutes_down = lv_img_create(ui_button_minutes_down);
    lv_img_set_src(ui_image_minutes_down, &ui_img_icon_down_png);
    lv_obj_set_align(ui_image_minutes_down, LV_ALIGN_CENTER);
    lv_img_set_zoom(ui_image_minutes_down, 170);

    ui_label_recording_countdown = lv_label_create(settings3_page);
    lv_obj_set_pos(ui_label_recording_countdown, -78, 0);
    lv_obj_set_align(ui_label_recording_countdown, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_recording_countdown, "Recording Countdown");
    lv_obj_add_style(ui_label_recording_countdown, &style_item_title, 0);

    ui_pannel_seconds = lv_obj_create(settings3_page);
    lv_obj_clear_flag(ui_pannel_seconds, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui_pannel_seconds, 45, 28);
    lv_obj_set_pos(ui_pannel_seconds, -83, 33);
    lv_obj_set_align(ui_pannel_seconds, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_pannel_seconds, &style_pannel, 0);
    ui_label_seconds_value = lv_label_create(ui_pannel_seconds);
    lv_obj_set_pos(ui_label_seconds_value, 1, 0);
    lv_obj_set_align(ui_label_seconds_value, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_seconds_value, "0");
    lv_obj_add_style(ui_label_seconds_value, &style_item_value, 0);

    ui_label_seconds = lv_label_create(settings3_page);
    lv_obj_set_pos(ui_label_seconds, -81, 56);
    lv_obj_set_align(ui_label_seconds, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_seconds, "seconds");
    lv_obj_add_style(ui_label_seconds, &style_label_unit, 0);

    ui_button_seconds_up = lv_btn_create(settings3_page);
    lv_obj_set_size(ui_button_seconds_up, 20, 20);
    lv_obj_set_pos(ui_button_seconds_up, -119, 33);
    lv_obj_set_align(ui_button_seconds_up, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_button_seconds_up, &style_button_primary, 0);
    lv_obj_add_style(ui_button_seconds_up, &style_button_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings3_page, ui_button_seconds_up);
    lv_obj_add_event_cb(ui_button_seconds_up, settings3_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_button_seconds_up, settings3_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    ui_image_seconds_up = lv_img_create(ui_button_seconds_up);
    lv_img_set_src(ui_image_seconds_up, &ui_img_icon_up_png);
    lv_obj_set_align(ui_image_seconds_up, LV_ALIGN_CENTER);
    lv_img_set_zoom(ui_image_seconds_up, 170);

    ui_button_seconds_down = lv_btn_create(settings3_page);
    lv_obj_set_size(ui_button_seconds_down, 20, 20);
    lv_obj_set_pos(ui_button_seconds_down, -46, 33);
    lv_obj_set_align(ui_button_seconds_down, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_button_seconds_down, &style_button_primary, 0);
    lv_obj_add_style(ui_button_seconds_down, &style_button_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings3_page, ui_button_seconds_down);
    lv_obj_add_event_cb(ui_button_seconds_down, settings3_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_button_seconds_down, settings3_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    ui_image_seconds_down = lv_img_create(ui_button_seconds_down);
    lv_img_set_src(ui_image_seconds_down, &ui_img_icon_down_png);
    lv_obj_set_align(ui_image_seconds_down, LV_ALIGN_CENTER);
    lv_img_set_zoom(ui_image_seconds_down, 170);

    ui_label_file_split_threshold = lv_label_create(settings3_page);
    lv_obj_set_pos(ui_label_file_split_threshold, 84, 0);
    lv_obj_set_align(ui_label_file_split_threshold, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_file_split_threshold, "File Split Threshold");
    lv_obj_add_style(ui_label_file_split_threshold, &style_item_title, 0);

    ui_pannel_mb = lv_obj_create(settings3_page);
    lv_obj_clear_flag(ui_pannel_mb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui_pannel_mb, 45, 28);
    lv_obj_set_pos(ui_pannel_mb, 88, 33);
    lv_obj_set_align(ui_pannel_mb, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_pannel_mb, &style_pannel, 0);
    ui_label_mb_value = lv_label_create(ui_pannel_mb);
    lv_obj_set_pos(ui_label_mb_value, 1, 0);
    lv_obj_set_align(ui_label_mb_value, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_mb_value, "0");
    lv_obj_add_style(ui_label_mb_value, &style_item_value, 0);

    ui_label_mb = lv_label_create(settings3_page);
    lv_obj_set_pos(ui_label_mb, 89, 56);
    lv_obj_set_align(ui_label_mb, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_mb, "MB");
    lv_obj_add_style(ui_label_mb, &style_label_unit, 0);

    ui_button_mb_up = lv_btn_create(settings3_page);
    lv_obj_set_size(ui_button_mb_up, 20, 20);
    lv_obj_set_pos(ui_button_mb_up, 52, 33);
    lv_obj_set_align(ui_button_mb_up, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_button_mb_up, &style_button_primary, 0);
    lv_obj_add_style(ui_button_mb_up, &style_button_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings3_page, ui_button_mb_up);
    lv_obj_add_event_cb(ui_button_mb_up, settings3_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_button_mb_up, settings3_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    ui_image_mb_up = lv_img_create(ui_button_mb_up);
    lv_img_set_src(ui_image_mb_up, &ui_img_icon_up_png);
    lv_obj_set_align(ui_image_mb_up, LV_ALIGN_CENTER);
    lv_img_set_zoom(ui_image_mb_up, 170);

    ui_button_mb_down = lv_btn_create(settings3_page);
    lv_obj_set_size(ui_button_mb_down, 20, 20);
    lv_obj_set_pos(ui_button_mb_down, 125, 33);
    lv_obj_set_align(ui_button_mb_down, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_button_mb_down, &style_button_primary, 0);
    lv_obj_add_style(ui_button_mb_down, &style_button_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings3_page, ui_button_mb_down);
    lv_obj_add_event_cb(ui_button_mb_down, settings3_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_button_mb_down, settings3_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    ui_image_mb_down = lv_img_create(ui_button_mb_down);
    lv_img_set_src(ui_image_mb_down, &ui_img_icon_down_png);
    lv_obj_set_align(ui_image_mb_down, LV_ALIGN_CENTER);
    lv_img_set_zoom(ui_image_mb_down, 170);

    ui_label_file_format = lv_label_create(settings3_page);
    lv_obj_set_pos(ui_label_file_format, -101, 94);
    lv_obj_set_align(ui_label_file_format, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_file_format, "File Format");
    lv_obj_add_style(ui_label_file_format, &style_item_title, 0);
    lv_obj_set_style_text_letter_space(ui_label_file_format, 2, 0);

    ui_checkbox_csv = lv_checkbox_create(settings3_page);
    lv_checkbox_set_text(ui_checkbox_csv, "CSV");
    lv_obj_set_pos(ui_checkbox_csv, -1, 95);
    lv_obj_set_align(ui_checkbox_csv, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_csv, &style_checkbox_label_value, 0);
    lv_obj_add_style(ui_checkbox_csv, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_csv, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_csv, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings3_page, ui_checkbox_csv);
    lv_obj_add_event_cb(ui_checkbox_csv, settings3_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_bdf = lv_checkbox_create(settings3_page);
    lv_checkbox_set_text(ui_checkbox_bdf, "BDF");
    lv_obj_set_pos(ui_checkbox_bdf, 83, 95);
    lv_obj_set_align(ui_checkbox_bdf, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_bdf, &style_checkbox_label_value, 0);
    lv_obj_add_style(ui_checkbox_bdf, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_bdf, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_bdf, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings3_page, ui_checkbox_bdf);
    lv_obj_add_event_cb(ui_checkbox_bdf, settings3_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_divider_line0 = lv_obj_create(settings3_page);
    lv_obj_set_size(ui_divider_line0, 2, 94);
    lv_obj_set_pos(ui_divider_line0, 7, 27);
    lv_obj_set_align(ui_divider_line0, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_divider_line0, &style_divider_line, 0);
    ui_divider_line1 = lv_obj_create(settings3_page);
    lv_obj_set_size(ui_divider_line1, 330, 2);
    lv_obj_set_pos(ui_divider_line1, 0, -19);
    lv_obj_set_align(ui_divider_line1, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_divider_line1, &style_divider_line, 0);
    ui_divider_line2 = lv_obj_create(settings3_page);
    lv_obj_set_size(ui_divider_line2, 330, 2);
    lv_obj_set_pos(ui_divider_line2, 0, 73);
    lv_obj_set_align(ui_divider_line2, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_divider_line2, &style_divider_line, 0);

    data_logger_t data_logger_status;
    data_logger_get_status_copy(&data_logger_status);
    // 初始化录制时长
    recording_duration_minutes = data_logger_status.user_defined_duration_min;
    int hours = recording_duration_minutes / 60;
    int minutes = recording_duration_minutes % 60;
    lv_label_set_text_fmt(ui_label_hours_value, "%d", hours);
    lv_label_set_text_fmt(ui_label_minutes_value, "%d", minutes);
    // 初始化录制倒计时
    uint8_t current_countdown = data_logger_status.prepare_countdown_sec;
    for (int i = 0; i < 8; i++) {
        if (recording_countdown_seconds_options[i] == current_countdown) {
            countdown_option_index = i;
            break;
        }
    }
    lv_label_set_text_fmt(ui_label_seconds_value, "%u", recording_countdown_seconds_options[countdown_option_index]);
    // 初始化文件分割阈值
    uint32_t current_split_size = data_logger_status.user_defined_split_size_mb;
    for (int i = 0; i < 7; i++) {
        if (file_split_size_options[i] == current_split_size) {
            split_size_option_index = i;
            break;
        }
    }
    lv_label_set_text_fmt(ui_label_mb_value, "%u", file_split_size_options[split_size_option_index]);
    // 初始化文件格式
    data_format_t current_format = data_logger_status.selected_format;
    if (current_format == DATA_FORMAT_CSV) {
        lv_obj_add_state(ui_checkbox_csv, LV_STATE_CHECKED);
        lv_obj_clear_state(ui_checkbox_bdf, LV_STATE_CHECKED);
    } else {
        lv_obj_add_state(ui_checkbox_bdf, LV_STATE_CHECKED);
        lv_obj_clear_state(ui_checkbox_csv, LV_STATE_CHECKED);
    }
}

void settings3_page_destroy(lv_obj_t *settings3_page) {
    (void)settings3_page;
    ui_label_recording_duration = NULL;
    ui_pannel_hours = NULL;
    ui_label_hours_value = NULL;
    ui_label_hours = NULL;
    ui_label_minutes = NULL;
    ui_button_hours_up = NULL;
    ui_image_hours_up = NULL;
    ui_button_hours_down = NULL;
    ui_image_hours_down = NULL;
    ui_pannel_minutes = NULL;
    ui_label_minutes_value = NULL;
    ui_button_minutes_up = NULL;
    ui_image_minutes_up = NULL;
    ui_button_minutes_down = NULL;
    ui_image_minutes_down = NULL;
    ui_label_recording_countdown = NULL;
    ui_pannel_seconds = NULL;
    ui_label_seconds_value = NULL;
    ui_label_seconds = NULL;
    ui_button_seconds_up = NULL;
    ui_image_seconds_up = NULL;
    ui_button_seconds_down = NULL;
    ui_image_seconds_down = NULL;
    ui_label_file_split_threshold = NULL;
    ui_pannel_mb = NULL;
    ui_label_mb_value = NULL;
    ui_label_mb = NULL;
    ui_button_mb_up = NULL;
    ui_image_mb_up = NULL;
    ui_button_mb_down = NULL;
    ui_image_mb_down = NULL;
    ui_label_file_format = NULL;
    ui_checkbox_csv = NULL;
    ui_checkbox_bdf = NULL;
    ui_divider_line0 = NULL;
    ui_divider_line1 = NULL;
    ui_divider_line2 = NULL;
    if (g_settings3_page) {
        lv_group_del(g_settings3_page);
        g_settings3_page = NULL;
    }
    destroy_vitrual_btn();
}

void settings3_page_willDisappear(lv_obj_t *settings3_page) { nvs_save_config(); }
