#include "settings1_page.h"
#include "main_page.h"
#include "mcu_fpga_comm.h"
#include "nvs_app.h"
#include "pm.h"
#include "ui_hal.h"
#include "ui_img_icon_rectangle_png.h"

LV_IMG_DECLARE(ui_img_icon_rectangle_png);
LV_FONT_DECLARE(ui_font_agencyb16);

static lv_obj_t *ui_checkbox_afe_hpf_0_05 = NULL;
static lv_obj_t *ui_checkbox_afe_hpf_1_5 = NULL;
static lv_obj_t *ui_checkbox_dsp_hpf_0_67 = NULL;
static lv_obj_t *ui_checkbox_dsp_lpf_40 = NULL;
static lv_obj_t *ui_checkbox_dsp_lpf_100 = NULL;
static lv_obj_t *ui_checkbox_dsp_lpf_200 = NULL;
static lv_obj_t *ui_checkbox_dsp_notch = NULL;
static lv_obj_t *ui_checkbox_paper_speed_12_5 = NULL;
static lv_obj_t *ui_checkbox_paper_speed_25 = NULL;
static lv_obj_t *ui_checkbox_paper_speed_50 = NULL;
static lv_obj_t *ui_checkbox_heartbeat_beep = NULL;
static lv_obj_t *ui_checkbox_system_beep = NULL;
static lv_obj_t *ui_label_afe_hpf = NULL;
static lv_obj_t *ui_label_dsp_hpf = NULL;
static lv_obj_t *ui_label_dsp_lpf = NULL;
static lv_obj_t *ui_label_dsp_notch = NULL;
static lv_obj_t *ui_label_system_beep = NULL;
static lv_obj_t *ui_label_heartbeat_beep = NULL;
static lv_obj_t *ui_label_paper_speed = NULL;
static lv_obj_t *ui_divider_line1 = NULL;
static lv_obj_t *ui_divider_line2 = NULL;
static lv_group_t *g_settings1_page = NULL;

lv_obj_t *afe_hpf_checkboxes[2];
lv_obj_t *dsp_lpf_checkboxes[3];
lv_obj_t *paper_speed_checkboxes[3];
static void handle_radio_group(lv_obj_t *active_checkbox, lv_obj_t *group[], uint32_t group_size) {
    // 单选组按钮不能通过再次点击来取消
    // 若再次点击已选中的单选框，导致其被“取消选中”，则应将其重新选中
    if (!lv_obj_has_state(active_checkbox, LV_STATE_CHECKED)) {
        lv_obj_add_state(active_checkbox, LV_STATE_CHECKED);
        return;
    }
    // 取消选中组中的所有其他复选框
    for (uint32_t i = 0; i < group_size; i++) {
        if (group[i] != active_checkbox) // 如果它不是刚刚被点击的那个
            lv_obj_clear_state(group[i], LV_STATE_CHECKED);
    }
}

static void settings1_event_cb(lv_event_t *e) {
    lv_obj_t *target_obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        // --- 单选组逻辑 ---
        if (target_obj == ui_checkbox_afe_hpf_0_05 || target_obj == ui_checkbox_afe_hpf_1_5) {
            handle_radio_group(target_obj, afe_hpf_checkboxes, 2);
            if (target_obj == ui_checkbox_afe_hpf_0_05) comm_set_afe_hpf(AFE_HPF_CUTOFF_0_05HZ);
            else comm_set_afe_hpf(AFE_HPF_CUTOFF_1_5HZ);
        } else if (target_obj == ui_checkbox_dsp_lpf_40 || target_obj == ui_checkbox_dsp_lpf_100 ||
                   target_obj == ui_checkbox_dsp_lpf_200) {
            handle_radio_group(target_obj, dsp_lpf_checkboxes, 3);
            if (target_obj == ui_checkbox_dsp_lpf_40) comm_set_lpf(LPF_CUTOFF_40HZ);
            else if (target_obj == ui_checkbox_dsp_lpf_100) comm_set_lpf(LPF_CUTOFF_100HZ);
            else comm_set_lpf(LPF_CUTOFF_200HZ);
        } else if (target_obj == ui_checkbox_paper_speed_12_5 || target_obj == ui_checkbox_paper_speed_25 ||
                   target_obj == ui_checkbox_paper_speed_50) {
            handle_radio_group(target_obj, paper_speed_checkboxes, 3);
            if (target_obj == ui_checkbox_paper_speed_12_5) set_ecg_paper_speed(ECG_SPEED_12_5_MM_S);
            else if (target_obj == ui_checkbox_paper_speed_25) set_ecg_paper_speed(ECG_SPEED_25_MM_S);
            else set_ecg_paper_speed(ECG_SPEED_50_MM_S);
            // --- 独立开关逻辑 ---
        } else if (target_obj == ui_checkbox_dsp_hpf_0_67) {
            bool is_checked = lv_obj_has_state(target_obj, LV_STATE_CHECKED);
            if (is_checked) comm_set_dsp_hpf(DSP_HPF_CUTOFF_0_67HZ);
            else comm_set_dsp_hpf(DSP_HPF_CUTOFF_0_05HZ);
        } else if (target_obj == ui_checkbox_dsp_notch) {
            bool is_checked = lv_obj_has_state(target_obj, LV_STATE_CHECKED);
            comm_set_notch(is_checked);
        } else if (target_obj == ui_checkbox_system_beep) {
            bool is_checked = lv_obj_has_state(target_obj, LV_STATE_CHECKED);
            set_system_beep(is_checked);
        } else if (target_obj == ui_checkbox_heartbeat_beep) {
            bool is_checked = lv_obj_has_state(target_obj, LV_STATE_CHECKED);
            set_heartbeat_beep(is_checked);
        }
    }
}

static void indev_cb(lv_event_t *e) {
    uint8_t btn_id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        lv_obj_t *focused_obj = lv_group_get_focused(g_settings1_page);
        switch (btn_id) {
        case 0: lv_group_focus_prev(g_settings1_page); break;
        case 1: lv_group_focus_next(g_settings1_page); break;
        case 2:
            btn_beep();
            lv_pm_open_options_t options = {.animation = LV_PM_ANIMA_SLIDE_LEFT, .target = LV_PM_TARGET_SELF};
            lv_pm_open_page(2, &options);
            break;
        case 3:
            if (focused_obj && lv_obj_check_type(focused_obj, &lv_checkbox_class)) {
                if (lv_obj_has_state(focused_obj, LV_STATE_CHECKED)) lv_obj_clear_state(focused_obj, LV_STATE_CHECKED);
                else lv_obj_add_state(focused_obj, LV_STATE_CHECKED);
                lv_event_send(focused_obj, LV_EVENT_VALUE_CHANGED, NULL);
            }
            break;
        default: break;
        }
        btn_beep();
    }
    if (code == LV_EVENT_LONG_PRESSED_REPEAT) {
        switch (btn_id) {
        case 0:
            lv_group_focus_prev(g_settings1_page);
            btn_beep();
            break;
        case 1:
            lv_group_focus_next(g_settings1_page);
            btn_beep();
            break;
        case 2:
            back_home_long_beep();
            lv_pm_open_options_t options = {.animation = LV_PM_ANIMA_SLIDE_RIGHT, .target = LV_PM_TARGET_SELF};
            lv_pm_open_page(0, &options);
            break;
        default: break;
        }
    }
}

void settings1_page_init(lv_obj_t *settings1_page) {
    create_vitrual_btn(settings1_page, indev_cb);
    lv_obj_set_style_bg_color(settings1_page, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(settings1_page, LV_OBJ_FLAG_SCROLLABLE);
    g_settings1_page = lv_group_create();
    lv_group_set_default(g_settings1_page);
    lv_indev_set_group(indev_button, g_settings1_page);

    static lv_style_t style_checkbox_indicator_default;
    lv_style_init(&style_checkbox_indicator_default);
    lv_style_set_radius(&style_checkbox_indicator_default, 0);
    lv_style_set_bg_color(&style_checkbox_indicator_default, lv_color_hex(0x000000));
    lv_style_set_border_color(&style_checkbox_indicator_default, lv_color_hex(0x03CB2B));
    static lv_style_t style_checkbox_indicator_checked;
    lv_style_init(&style_checkbox_indicator_checked);
    lv_style_set_radius(&style_checkbox_indicator_checked, 0);
    lv_style_set_bg_color(&style_checkbox_indicator_checked, lv_color_hex(0x000000));
    lv_style_set_bg_img_src(&style_checkbox_indicator_checked, &ui_img_icon_rectangle_png);
    static lv_style_t style_checkbox_main;
    lv_style_init(&style_checkbox_main);
    lv_style_set_text_color(&style_checkbox_main, lv_color_hex(0x03CB2B));
    lv_style_set_text_font(&style_checkbox_main, &lv_font_montserrat_12);
    static lv_style_t style_checkbox_focused;
    lv_style_init(&style_checkbox_focused);
    lv_style_set_border_color(&style_checkbox_focused, lv_color_hex(0xFF9800));
    static lv_style_t style_label_main;
    lv_style_init(&style_label_main);
    lv_style_set_text_color(&style_label_main, lv_color_hex(0x03CB2B));
    lv_style_set_text_font(&style_label_main, &ui_font_agencyb16);
    lv_style_set_text_letter_space(&style_label_main, 1);

    ui_checkbox_afe_hpf_0_05 = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_afe_hpf_0_05, "0.05");
    lv_obj_set_pos(ui_checkbox_afe_hpf_0_05, 5, -94);
    lv_obj_set_align(ui_checkbox_afe_hpf_0_05, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_afe_hpf_0_05, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_afe_hpf_0_05, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_afe_hpf_0_05, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_afe_hpf_0_05, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_afe_hpf_0_05);
    afe_hpf_checkboxes[0] = ui_checkbox_afe_hpf_0_05;
    lv_obj_add_event_cb(ui_checkbox_afe_hpf_0_05, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_afe_hpf_1_5 = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_afe_hpf_1_5, "1.5");
    lv_obj_set_pos(ui_checkbox_afe_hpf_1_5, 62, -94);
    lv_obj_set_align(ui_checkbox_afe_hpf_1_5, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_afe_hpf_1_5, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_afe_hpf_1_5, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_afe_hpf_1_5, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_afe_hpf_1_5, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_afe_hpf_1_5);
    afe_hpf_checkboxes[1] = ui_checkbox_afe_hpf_1_5;
    lv_obj_add_event_cb(ui_checkbox_afe_hpf_1_5, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_dsp_hpf_0_67 = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_dsp_hpf_0_67, "0.67");
    lv_obj_set_pos(ui_checkbox_dsp_hpf_0_67, 5, -65);
    lv_obj_set_align(ui_checkbox_dsp_hpf_0_67, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_dsp_hpf_0_67, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_dsp_hpf_0_67, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_dsp_hpf_0_67, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_dsp_hpf_0_67, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_dsp_hpf_0_67);
    lv_obj_add_event_cb(ui_checkbox_dsp_hpf_0_67, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_dsp_lpf_40 = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_dsp_lpf_40, " 40");
    lv_obj_set_pos(ui_checkbox_dsp_lpf_40, 2, -36);
    lv_obj_set_align(ui_checkbox_dsp_lpf_40, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_dsp_lpf_40, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_dsp_lpf_40, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_dsp_lpf_40, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_dsp_lpf_40, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_dsp_lpf_40);
    dsp_lpf_checkboxes[0] = ui_checkbox_dsp_lpf_40;
    lv_obj_add_event_cb(ui_checkbox_dsp_lpf_40, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_dsp_lpf_100 = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_dsp_lpf_100, "100");
    lv_obj_set_pos(ui_checkbox_dsp_lpf_100, 64, -36);
    lv_obj_set_align(ui_checkbox_dsp_lpf_100, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_dsp_lpf_100, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_dsp_lpf_100, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_dsp_lpf_100, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_dsp_lpf_100, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_dsp_lpf_100);
    dsp_lpf_checkboxes[1] = ui_checkbox_dsp_lpf_100;
    lv_obj_add_event_cb(ui_checkbox_dsp_lpf_100, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_dsp_lpf_200 = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_dsp_lpf_200, "200");
    lv_obj_set_pos(ui_checkbox_dsp_lpf_200, 125, -36);
    lv_obj_set_align(ui_checkbox_dsp_lpf_200, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_dsp_lpf_200, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_dsp_lpf_200, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_dsp_lpf_200, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_dsp_lpf_200, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_dsp_lpf_200);
    dsp_lpf_checkboxes[2] = ui_checkbox_dsp_lpf_200;
    lv_obj_add_event_cb(ui_checkbox_dsp_lpf_200, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_dsp_notch = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_dsp_notch, "ON/OFF");
    lv_obj_set_pos(ui_checkbox_dsp_notch, 17, -7);
    lv_obj_set_align(ui_checkbox_dsp_notch, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_dsp_notch, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_dsp_notch, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_dsp_notch, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_dsp_notch, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_dsp_notch);
    lv_obj_add_event_cb(ui_checkbox_dsp_notch, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_system_beep = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_system_beep, "ON/OFF");
    lv_obj_set_pos(ui_checkbox_system_beep, 17, 30);
    lv_obj_set_align(ui_checkbox_system_beep, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_system_beep, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_system_beep, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_system_beep, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_system_beep, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_system_beep);
    lv_obj_add_event_cb(ui_checkbox_system_beep, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_heartbeat_beep = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_heartbeat_beep, "ON/OFF");
    lv_obj_set_pos(ui_checkbox_heartbeat_beep, 17, 59);
    lv_obj_set_align(ui_checkbox_heartbeat_beep, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_heartbeat_beep, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_heartbeat_beep, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_heartbeat_beep, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_heartbeat_beep, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_heartbeat_beep);
    lv_obj_add_event_cb(ui_checkbox_heartbeat_beep, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_paper_speed_12_5 = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_paper_speed_12_5, "12.5");
    lv_obj_set_pos(ui_checkbox_paper_speed_12_5, 3, 94);
    lv_obj_set_align(ui_checkbox_paper_speed_12_5, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_paper_speed_12_5, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_paper_speed_12_5, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_paper_speed_12_5, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_paper_speed_12_5, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_paper_speed_12_5);
    paper_speed_checkboxes[0] = ui_checkbox_paper_speed_12_5;
    lv_obj_add_event_cb(ui_checkbox_paper_speed_12_5, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_paper_speed_25 = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_paper_speed_25, "25");
    lv_obj_set_pos(ui_checkbox_paper_speed_25, 62, 94);
    lv_obj_set_align(ui_checkbox_paper_speed_25, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_paper_speed_25, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_paper_speed_25, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_paper_speed_25, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_paper_speed_25, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_paper_speed_25);
    paper_speed_checkboxes[1] = ui_checkbox_paper_speed_25;
    lv_obj_add_event_cb(ui_checkbox_paper_speed_25, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_paper_speed_50 = lv_checkbox_create(settings1_page);
    lv_checkbox_set_text(ui_checkbox_paper_speed_50, "50");
    lv_obj_set_pos(ui_checkbox_paper_speed_50, 121, 94);
    lv_obj_set_align(ui_checkbox_paper_speed_50, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_paper_speed_50, &style_checkbox_main, 0);
    lv_obj_add_style(ui_checkbox_paper_speed_50, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_paper_speed_50, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_paper_speed_50, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings1_page, ui_checkbox_paper_speed_50);
    paper_speed_checkboxes[2] = ui_checkbox_paper_speed_50;
    lv_obj_add_event_cb(ui_checkbox_paper_speed_50, settings1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_label_afe_hpf = lv_label_create(settings1_page);
    lv_obj_set_pos(ui_label_afe_hpf, -98, -94);
    lv_obj_set_align(ui_label_afe_hpf, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_afe_hpf, "AFE HPF [Hz]");
    lv_obj_add_style(ui_label_afe_hpf, &style_label_main, 0);

    ui_label_dsp_hpf = lv_label_create(settings1_page);
    lv_obj_set_pos(ui_label_dsp_hpf, -98, -65);
    lv_obj_set_align(ui_label_dsp_hpf, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_dsp_hpf, "DSP HPF [Hz]");
    lv_obj_add_style(ui_label_dsp_hpf, &style_label_main, 0);

    ui_label_dsp_lpf = lv_label_create(settings1_page);
    lv_obj_set_pos(ui_label_dsp_lpf, -98, -36);
    lv_obj_set_align(ui_label_dsp_lpf, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_dsp_lpf, "DSP LPF [Hz]");
    lv_obj_add_style(ui_label_dsp_lpf, &style_label_main, 0);

    ui_label_dsp_notch = lv_label_create(settings1_page);
    lv_obj_set_pos(ui_label_dsp_notch, -94, -7);
    lv_obj_set_align(ui_label_dsp_notch, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_dsp_notch, "DSP Notch [50Hz]");
    lv_obj_add_style(ui_label_dsp_notch, &style_label_main, 0);

    ui_label_system_beep = lv_label_create(settings1_page);
    lv_obj_set_pos(ui_label_system_beep, -100, 30);
    lv_obj_set_align(ui_label_system_beep, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_system_beep, "System  Beep");
    lv_obj_add_style(ui_label_system_beep, &style_label_main, 0);

    ui_label_heartbeat_beep = lv_label_create(settings1_page);
    lv_obj_set_pos(ui_label_heartbeat_beep, -100, 59);
    lv_obj_set_align(ui_label_heartbeat_beep, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_heartbeat_beep, "Heartbeat  Beep");
    lv_obj_add_style(ui_label_heartbeat_beep, &style_label_main, 0);

    ui_label_paper_speed = lv_label_create(settings1_page);
    lv_obj_set_pos(ui_label_paper_speed, -95, 94);
    lv_obj_set_align(ui_label_paper_speed, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_paper_speed, "Paper Speed  [mm/s]");
    lv_obj_add_style(ui_label_paper_speed, &style_label_main, 0);
    lv_obj_set_style_text_letter_space(ui_label_paper_speed, 0, 0);

    ui_divider_line1 = lv_obj_create(settings1_page);
    lv_obj_set_size(ui_divider_line1, 330, 2);
    lv_obj_set_pos(ui_divider_line1, 0, 12);
    lv_obj_set_align(ui_divider_line1, LV_ALIGN_CENTER);
    lv_obj_set_style_border_color(ui_divider_line1, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_opa(ui_divider_line1, 230, 0);

    ui_divider_line2 = lv_obj_create(settings1_page);
    lv_obj_set_size(ui_divider_line2, 330, 2);
    lv_obj_set_pos(ui_divider_line2, 0, 77);
    lv_obj_set_align(ui_divider_line2, LV_ALIGN_CENTER);
    lv_obj_set_style_border_color(ui_divider_line2, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_opa(ui_divider_line2, 230, 0);

    if (comm_get_afe_hpf() == AFE_HPF_CUTOFF_0_05HZ) handle_radio_group(ui_checkbox_afe_hpf_0_05, afe_hpf_checkboxes, 2);
    else handle_radio_group(ui_checkbox_afe_hpf_1_5, afe_hpf_checkboxes, 2);
    if (comm_get_dsp_hpf() == DSP_HPF_CUTOFF_0_67HZ) lv_obj_add_state(ui_checkbox_dsp_hpf_0_67, LV_STATE_CHECKED);
    else lv_obj_add_state(ui_checkbox_dsp_hpf_0_67, LV_STATE_DEFAULT);
    switch (comm_get_lpf()) {
    case LPF_CUTOFF_40HZ: handle_radio_group(ui_checkbox_dsp_lpf_40, dsp_lpf_checkboxes, 3); break;
    case LPF_CUTOFF_100HZ: handle_radio_group(ui_checkbox_dsp_lpf_100, dsp_lpf_checkboxes, 3); break;
    case LPF_CUTOFF_200HZ: handle_radio_group(ui_checkbox_dsp_lpf_200, dsp_lpf_checkboxes, 3); break;
    default: handle_radio_group(ui_checkbox_dsp_lpf_40, dsp_lpf_checkboxes, 3); break;
    }
    if (comm_get_notch()) lv_obj_add_state(ui_checkbox_dsp_notch, LV_STATE_CHECKED);
    else lv_obj_add_state(ui_checkbox_dsp_notch, LV_STATE_DEFAULT);
    if (get_system_beep()) lv_obj_add_state(ui_checkbox_system_beep, LV_STATE_CHECKED);
    else lv_obj_add_state(ui_checkbox_system_beep, LV_STATE_DEFAULT);
    if (get_heartbeat_beep()) lv_obj_add_state(ui_checkbox_heartbeat_beep, LV_STATE_CHECKED);
    else lv_obj_add_state(ui_checkbox_heartbeat_beep, LV_STATE_DEFAULT);
    switch (get_ecg_paper_speed()) {
    case 0: handle_radio_group(ui_checkbox_paper_speed_12_5, paper_speed_checkboxes, 3); break;
    case 1: handle_radio_group(ui_checkbox_paper_speed_25, paper_speed_checkboxes, 3); break;
    case 2: handle_radio_group(ui_checkbox_paper_speed_50, paper_speed_checkboxes, 3); break;
    default: handle_radio_group(ui_checkbox_paper_speed_25, paper_speed_checkboxes, 3); break;
    }
}

void settings1_page_destroy(lv_obj_t *settings1_page) {
    (void)settings1_page;
    ui_checkbox_afe_hpf_0_05 = NULL;
    ui_checkbox_afe_hpf_1_5 = NULL;
    ui_checkbox_dsp_hpf_0_67 = NULL;
    ui_checkbox_dsp_lpf_40 = NULL;
    ui_checkbox_dsp_lpf_100 = NULL;
    ui_checkbox_dsp_lpf_200 = NULL;
    ui_checkbox_dsp_notch = NULL;
    ui_checkbox_paper_speed_12_5 = NULL;
    ui_checkbox_paper_speed_25 = NULL;
    ui_checkbox_paper_speed_50 = NULL;
    ui_checkbox_heartbeat_beep = NULL;
    ui_checkbox_system_beep = NULL;
    ui_label_afe_hpf = NULL;
    ui_label_dsp_hpf = NULL;
    ui_label_dsp_lpf = NULL;
    ui_label_dsp_notch = NULL;
    ui_label_system_beep = NULL;
    ui_label_heartbeat_beep = NULL;
    ui_label_paper_speed = NULL;
    ui_divider_line1 = NULL;
    ui_divider_line2 = NULL;
    if (g_settings1_page) {
        lv_group_del(g_settings1_page);
        g_settings1_page = NULL;
    }
    destroy_vitrual_btn();
}

void settings1_page_willDisappear(lv_obj_t *settings1_page) { nvs_save_config(); }
