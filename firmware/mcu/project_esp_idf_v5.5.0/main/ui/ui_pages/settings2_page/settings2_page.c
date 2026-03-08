#include "settings2_page.h"
#include "app_file_server.h"
#include "data_logger.h"
#include "nvs_app.h"
#include "pm.h"
#include "ui_hal.h"
#include "ui_img_icon_rectangle2_png.h"
#include "wifi_app.h"

LV_IMG_DECLARE(ui_img_icon_rectangle2_png);
LV_FONT_DECLARE(ui_font_agencyb17);

static lv_obj_t *ui_label_network_autostart = NULL;
static lv_obj_t *ui_checkbox_network_autostart = NULL;
static lv_obj_t *ui_label_file_server_wifi_mode = NULL;
static lv_obj_t *ui_checkbox_file_server_sta_mode = NULL;
static lv_obj_t *ui_checkbox_file_server_ap_mode = NULL;
static lv_obj_t *ui_button_stop_wifi = NULL;
static lv_obj_t *ui_label_stop_wifi = NULL;
static lv_obj_t *ui_button_change_wifi = NULL;
static lv_obj_t *ui_label_change_wifi = NULL;
static lv_obj_t *ui_label_wifi_ssid = NULL;
static lv_obj_t *ui_label_wifi_ip = NULL;
static lv_obj_t *ui_label_card_size = NULL;
static lv_obj_t *ui_label_wifi_ssid_value = NULL;
static lv_obj_t *ui_label_wifi_ip_value = NULL;
static lv_obj_t *ui_label_card_size_value = NULL;
static lv_group_t *g_settings2_page = NULL;
static int countdown_ticks = 0;

static void indev_cb(lv_event_t *e) {
    uint8_t btn_id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *focused_obj = lv_group_get_focused(g_settings2_page);
    if (code == LV_EVENT_PRESSED) {
        if (lv_obj_check_type(focused_obj, &lv_btn_class) && btn_id == 3) {
            lv_obj_add_state(focused_obj, LV_STATE_PRESSED);
        }
    }
    if (code == LV_EVENT_CLICKED) {
        switch (btn_id) {
        case 0: lv_group_focus_prev(g_settings2_page); break;
        case 1: lv_group_focus_next(g_settings2_page); break;
        case 2:
            btn_beep();
            lv_pm_open_options_t options = {.animation = LV_PM_ANIMA_SLIDE_LEFT, .target = LV_PM_TARGET_SELF};
            lv_pm_open_page(3, &options);
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
        if (btn_id == 2) {
            back_home_long_beep();
            lv_pm_open_options_t options = {.animation = LV_PM_ANIMA_SLIDE_RIGHT, .target = LV_PM_TARGET_SELF};
            lv_pm_open_page(0, &options);
        }
    }
}

static void handle_wifi_mode_checkbox(lv_obj_t *active_checkbox) {
    // 单选组按钮不能通过再次点击来取消
    // 若再次点击已选中的单选框，导致其被“取消选中”，则应将其重新选中
    if (!lv_obj_has_state(active_checkbox, LV_STATE_CHECKED)) {
        lv_obj_add_state(active_checkbox, LV_STATE_CHECKED);
        return;
    }
    // 如果它不是刚刚被点击的那个，则除去选定态
    if (ui_checkbox_file_server_sta_mode != active_checkbox) {
        lv_obj_clear_state(ui_checkbox_file_server_sta_mode, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_checkbox_file_server_ap_mode, LV_STATE_CHECKED);
    }
}

static lv_obj_t *busy_msgbox = NULL;
static void show_busy_msgbox(void) {
    if (busy_msgbox) return;
    countdown_ticks = 6;
    char buf[64];
    snprintf(buf, sizeof(buf), "Resource is busy     (%ds)", (countdown_ticks + 1) / 2);
    busy_msgbox = lv_msgbox_create(NULL, "Notice", buf, NULL, false);
    lv_obj_t *title_label = lv_msgbox_get_title(busy_msgbox);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x000000), 0);
    lv_obj_t *content_label = lv_msgbox_get_text(busy_msgbox);
    lv_obj_set_style_text_font(content_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(content_label, lv_color_hex(0x000000), 0);
    lv_obj_center(busy_msgbox);
}

static lv_obj_t *wifi_pairing_msgbox = NULL;
static void show_wifi_pairing_msgbox(void) {
    if (wifi_pairing_msgbox) return;
    countdown_ticks = 8;
    char buf[64];
    snprintf(buf, sizeof(buf), "Entering Wi-Fi Pairing Mode...(%ds)", (countdown_ticks + 1) / 2);
    wifi_pairing_msgbox = lv_msgbox_create(NULL, "Notice", buf, NULL, false);
    lv_obj_t *content_label = lv_msgbox_get_text(wifi_pairing_msgbox);
    lv_obj_set_style_text_font(content_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(content_label, lv_color_hex(0x000000), 0);
    lv_obj_center(wifi_pairing_msgbox);
}

static bool setting_network_autostart = true;
bool settings2_page_get_autostart_setting(void) { return setting_network_autostart; }
void settings2_page_set_autostart_setting(bool autostart_setting) { setting_network_autostart = autostart_setting; }
static void settings2_event_cb(lv_event_t *e) {
    lv_obj_t *target_obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        // --- 单选组逻辑 ---
        if (target_obj == ui_checkbox_file_server_sta_mode || target_obj == ui_checkbox_file_server_ap_mode) {
            if (!acquire_file_server_lock() || wifi_status.is_provisioning) {
                if (lv_obj_has_state(target_obj, LV_STATE_CHECKED)) lv_obj_clear_state(target_obj, LV_STATE_CHECKED);
                else lv_obj_add_state(target_obj, LV_STATE_CHECKED);
                show_busy_msgbox();
                return;
            } else {
                handle_wifi_mode_checkbox(target_obj);
                wifi_operating_mode_t current_mode = get_wifi_mode();
                wifi_operating_mode_t selected_mode = (target_obj == ui_checkbox_file_server_sta_mode) ? WIFI_STA : WIFI_AP;
                if (current_mode != selected_mode) {
                    set_wifi_mode(selected_mode);
                    stop_network_services();
                    start_network_services(true);
                }
                release_file_server_lock();
            }
        }
        // --- 独立开关逻辑 ---
        else if (target_obj == ui_checkbox_network_autostart) {
            setting_network_autostart = lv_obj_has_state(target_obj, LV_STATE_CHECKED);
        }
    } else if (code == LV_EVENT_CLICKED) {
        if (target_obj == ui_button_change_wifi) {
            if (!acquire_file_server_lock()) {
                show_busy_msgbox();
                return;
            } else {
                if ((wifi_status.is_start) && (!wifi_status.is_provisioning)) {
                    show_wifi_pairing_msgbox();
                    restart_wifi_provisioning();
                }
                release_file_server_lock();
            }
        } else if (target_obj == ui_button_stop_wifi) {
            if (!acquire_file_server_lock()) {
                show_busy_msgbox();
                return;
            } else {
                if (wifi_status.is_start) {
                    // 关闭wifi
                    lv_obj_set_style_bg_color(ui_button_stop_wifi, lv_color_hex(0xA4DEA4), 0);
                    lv_label_set_text(ui_label_stop_wifi, "Start Wi-Fi");
                    lv_obj_add_state(ui_button_change_wifi, LV_STATE_DISABLED); // wifi关闭时禁用配网按钮
                    stop_network_services();
                } else {
                    // 开启wifi
                    lv_obj_set_style_bg_color(ui_button_stop_wifi, lv_color_hex(0x4DDD66), 0);
                    lv_label_set_text(ui_label_stop_wifi, "Stop Wi-Fi");
                    lv_obj_clear_state(ui_button_change_wifi, LV_STATE_DISABLED);
                    start_network_services(true);
                }
                release_file_server_lock();
            }
        }
    }
}

void settings2_page_init(lv_obj_t *settings2_page) {
    create_vitrual_btn(settings2_page, indev_cb);
    lv_obj_set_style_bg_color(settings2_page, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(settings2_page, LV_OBJ_FLAG_SCROLLABLE);
    g_settings2_page = lv_group_create();
    lv_group_set_default(g_settings2_page);
    lv_indev_set_group(indev_button, g_settings2_page);

    static lv_style_t style_label_title;
    lv_style_init(&style_label_title);
    lv_style_set_text_color(&style_label_title, lv_color_hex(0x03CB2B));
    lv_style_set_text_letter_space(&style_label_title, 1);
    lv_style_set_text_font(&style_label_title, &ui_font_agencyb17);
    static lv_style_t style_label_value;
    lv_style_init(&style_label_value);
    lv_style_set_text_color(&style_label_value, lv_color_hex(0x03CB2B));
    lv_style_set_text_letter_space(&style_label_value, 1);
    lv_style_set_text_font(&style_label_value, &lv_font_montserrat_14);
    static lv_style_t style_button_primary;
    lv_style_init(&style_button_primary);
    lv_style_set_radius(&style_button_primary, 6);
    lv_style_set_bg_color(&style_button_primary, lv_color_hex(0x4DDD66));
    lv_style_set_shadow_color(&style_button_primary, lv_color_hex(0x000000));
    lv_style_set_transform_zoom(&style_button_primary, 200);
    static lv_style_t style_button_focused;
    lv_style_init(&style_button_focused);
    lv_style_set_border_width(&style_button_focused, 3);
    lv_style_set_border_color(&style_button_focused, lv_color_hex(0x000000));
    lv_style_set_outline_width(&style_button_focused, 4);
    lv_style_set_outline_color(&style_button_focused, lv_color_hex(0xFF9800));
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

    ui_label_network_autostart = lv_label_create(settings2_page);
    lv_obj_set_pos(ui_label_network_autostart, -74, -94);
    lv_obj_set_align(ui_label_network_autostart, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_network_autostart, "Network Auto Start");
    lv_obj_add_style(ui_label_network_autostart, &style_label_title, 0);

    ui_checkbox_network_autostart = lv_checkbox_create(settings2_page);
    lv_checkbox_set_text(ui_checkbox_network_autostart, "");
    lv_obj_set_pos(ui_checkbox_network_autostart, 43, -93);
    lv_obj_set_align(ui_checkbox_network_autostart, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_network_autostart, &style_label_value, 0);
    lv_obj_add_style(ui_checkbox_network_autostart, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_network_autostart, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_network_autostart, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings2_page, ui_checkbox_network_autostart);
    lv_obj_add_event_cb(ui_checkbox_network_autostart, settings2_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_label_file_server_wifi_mode = lv_label_create(settings2_page);
    lv_obj_set_pos(ui_label_file_server_wifi_mode, -67, -62);
    lv_obj_set_align(ui_label_file_server_wifi_mode, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_file_server_wifi_mode, "File Server Wi-Fi Mode");
    lv_obj_add_style(ui_label_file_server_wifi_mode, &style_label_title, 0);

    ui_checkbox_file_server_sta_mode = lv_checkbox_create(settings2_page);
    lv_checkbox_set_text(ui_checkbox_file_server_sta_mode, "STA");
    lv_obj_set_pos(ui_checkbox_file_server_sta_mode, 57, -61);
    lv_obj_set_align(ui_checkbox_file_server_sta_mode, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_file_server_sta_mode, &style_label_value, 0);
    lv_obj_add_style(ui_checkbox_file_server_sta_mode, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_file_server_sta_mode, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_file_server_sta_mode, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings2_page, ui_checkbox_file_server_sta_mode);
    lv_obj_add_event_cb(ui_checkbox_file_server_sta_mode, settings2_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_checkbox_file_server_ap_mode = lv_checkbox_create(settings2_page);
    lv_checkbox_set_text(ui_checkbox_file_server_ap_mode, "AP");
    lv_obj_set_pos(ui_checkbox_file_server_ap_mode, 117, -61);
    lv_obj_set_align(ui_checkbox_file_server_ap_mode, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_checkbox_file_server_ap_mode, &style_label_value, 0);
    lv_obj_add_style(ui_checkbox_file_server_ap_mode, &style_checkbox_indicator_default, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_checkbox_file_server_ap_mode, &style_checkbox_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_checkbox_file_server_ap_mode, &style_checkbox_focused, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings2_page, ui_checkbox_file_server_ap_mode);
    lv_obj_add_event_cb(ui_checkbox_file_server_ap_mode, settings2_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_button_stop_wifi = lv_btn_create(settings2_page);
    lv_obj_set_size(ui_button_stop_wifi, 175, 45);
    lv_obj_set_pos(ui_button_stop_wifi, -58, -6);
    lv_obj_set_align(ui_button_stop_wifi, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_button_stop_wifi, &style_button_primary, 0);
    lv_obj_add_style(ui_button_stop_wifi, &style_button_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings2_page, ui_button_stop_wifi);
    lv_obj_add_event_cb(ui_button_stop_wifi, settings2_event_cb, LV_EVENT_CLICKED, NULL);

    ui_label_stop_wifi = lv_label_create(settings2_page);
    lv_obj_set_pos(ui_label_stop_wifi, -78, -11);
    lv_obj_set_align(ui_label_stop_wifi, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_stop_wifi, "Stop Wi-Fi");
    lv_obj_set_style_text_color(ui_label_stop_wifi, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(ui_label_stop_wifi, &lv_font_montserrat_16, 0);

    ui_button_change_wifi = lv_btn_create(settings2_page);
    lv_obj_set_size(ui_button_change_wifi, 175, 45);
    lv_obj_align(ui_button_change_wifi, LV_ALIGN_CENTER, 94, -6);
    lv_obj_add_style(ui_button_change_wifi, &style_button_primary, 0);
    lv_obj_add_style(ui_button_change_wifi, &style_button_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(g_settings2_page, ui_button_change_wifi);
    lv_obj_add_event_cb(ui_button_change_wifi, settings2_event_cb, LV_EVENT_CLICKED, NULL);

    ui_label_change_wifi = lv_label_create(settings2_page);
    lv_obj_set_pos(ui_label_change_wifi, 75, -11);
    lv_obj_set_align(ui_label_change_wifi, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_change_wifi, "Change Wi-Fi");
    lv_obj_set_style_text_color(ui_label_change_wifi, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(ui_label_change_wifi, &lv_font_montserrat_16, 0);

    ui_label_wifi_ssid = lv_label_create(settings2_page);
    lv_obj_set_pos(ui_label_wifi_ssid, -88, 42);
    lv_obj_set_align(ui_label_wifi_ssid, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_wifi_ssid, "Wi-Fi SSID");
    lv_obj_add_style(ui_label_wifi_ssid, &style_label_title, 0);

    ui_label_wifi_ssid_value = lv_label_create(settings2_page);
    lv_obj_set_width(ui_label_wifi_ssid_value, 169);
    lv_obj_set_pos(ui_label_wifi_ssid_value, 80, 42);
    lv_obj_set_align(ui_label_wifi_ssid_value, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_label_wifi_ssid_value, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(ui_label_wifi_ssid_value, "----");
    lv_obj_add_style(ui_label_wifi_ssid_value, &style_label_value, 0);

    ui_label_wifi_ip = lv_label_create(settings2_page);
    lv_obj_set_pos(ui_label_wifi_ip, -97, 68);
    lv_obj_set_align(ui_label_wifi_ip, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_wifi_ip, "Wi-Fi IP");
    lv_obj_add_style(ui_label_wifi_ip, &style_label_title, 0);

    ui_label_wifi_ip_value = lv_label_create(settings2_page);
    lv_obj_set_width(ui_label_wifi_ip_value, 169);
    lv_obj_set_pos(ui_label_wifi_ip_value, 80, 68);
    lv_obj_set_align(ui_label_wifi_ip_value, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_label_wifi_ip_value, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(ui_label_wifi_ip_value, "--.--.--.--");
    lv_obj_add_style(ui_label_wifi_ip_value, &style_label_value, 0);
    lv_obj_set_style_text_letter_space(ui_label_wifi_ip_value, 2, 0);

    ui_label_card_size = lv_label_create(settings2_page);
    lv_obj_set_pos(ui_label_card_size, -89, 94);
    lv_obj_set_align(ui_label_card_size, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_card_size, "Card Size");
    lv_obj_add_style(ui_label_card_size, &style_label_title, 0);

    ui_label_card_size_value = lv_label_create(settings2_page);
    lv_obj_set_width(ui_label_card_size_value, 169);
    lv_obj_set_pos(ui_label_card_size_value, 80, 94);
    lv_obj_set_align(ui_label_card_size_value, LV_ALIGN_CENTER);

    char cap_str_buf[30];
    data_logger_t data_logger_status;
    data_logger_get_status_copy(&data_logger_status);
    if (data_logger_status.last_error == LOGGER_ERROR_NONE && data_logger_status.is_sdcard_present) {
        uint32_t used_mb = (uint32_t)((uint64_t)data_logger_status.sdcard_used_space_bytes / (1024 * 1024));
        uint32_t total_mb = (uint32_t)((uint64_t)data_logger_status.sdcard_total_space_bytes / (1024 * 1024));
        snprintf(cap_str_buf, sizeof(cap_str_buf), "%ld/%ld MB", (unsigned long)used_mb, (unsigned long)total_mb);
        lv_label_set_text(ui_label_card_size_value, cap_str_buf);
    } else lv_label_set_text(ui_label_card_size_value, "--/-- MB");
    lv_obj_add_style(ui_label_card_size_value, &style_label_value, 0);

    if (setting_network_autostart) lv_obj_add_state(ui_checkbox_network_autostart, LV_STATE_CHECKED);
    else lv_obj_clear_state(ui_checkbox_network_autostart, LV_STATE_CHECKED);

    if (get_wifi_mode() == WIFI_STA) handle_wifi_mode_checkbox(ui_checkbox_file_server_sta_mode);
    else handle_wifi_mode_checkbox(ui_checkbox_file_server_ap_mode);

    if (wifi_status.is_start) {
        lv_obj_set_style_bg_color(ui_button_stop_wifi, lv_color_hex(0x4DDD66), 0);
        lv_label_set_text(ui_label_stop_wifi, "Stop Wi-Fi");
        lv_obj_clear_state(ui_button_change_wifi, LV_STATE_DISABLED);
    } else {
        lv_obj_set_style_bg_color(ui_button_stop_wifi, lv_color_hex(0xA4DEA4), 0);
        lv_label_set_text(ui_label_stop_wifi, "Start Wi-Fi");
        lv_obj_add_state(ui_button_change_wifi, LV_STATE_DISABLED); // wifi关闭时禁用配网按钮
    }
}

void settings2_page_destroy(lv_obj_t *settings2_page) {
    (void)settings2_page;
    ui_label_network_autostart = NULL;
    ui_checkbox_network_autostart = NULL;
    ui_label_file_server_wifi_mode = NULL;
    ui_checkbox_file_server_sta_mode = NULL;
    ui_checkbox_file_server_ap_mode = NULL;
    ui_button_stop_wifi = NULL;
    ui_label_stop_wifi = NULL;
    ui_button_change_wifi = NULL;
    ui_label_change_wifi = NULL;
    ui_label_wifi_ssid = NULL;
    ui_label_wifi_ip = NULL;
    ui_label_card_size = NULL;
    ui_label_wifi_ssid_value = NULL;
    ui_label_wifi_ip_value = NULL;
    ui_label_card_size_value = NULL;
    if (g_settings2_page) {
        lv_group_del(g_settings2_page);
        g_settings2_page = NULL;
    }
    destroy_vitrual_btn();
}

void settings2_page_timer_cb(lv_timer_t *timer) {
    if (busy_msgbox) {
        countdown_ticks--;
        if (countdown_ticks > 0) {
            lv_obj_t *content_label = lv_msgbox_get_text(busy_msgbox);
            if (content_label) {
                int remaining_sec = (countdown_ticks + 1) / 2;
                lv_label_set_text_fmt(content_label, "Resource is busy     (%ds)", remaining_sec);
            }
        } else {
            lv_msgbox_close(busy_msgbox);
            busy_msgbox = NULL;
        }
    } else if (wifi_pairing_msgbox) {
        countdown_ticks--;
        if (countdown_ticks > 0) {
            lv_obj_t *content_label = lv_msgbox_get_text(wifi_pairing_msgbox);
            if (content_label) {
                int remaining_sec = (countdown_ticks + 1) / 2;
                lv_label_set_text_fmt(content_label, "Entering Wi-Fi Pairing Mode...(%ds)", remaining_sec);
            }
        } else {
            lv_msgbox_close(wifi_pairing_msgbox);
            wifi_pairing_msgbox = NULL;
        }
    }
    lv_label_set_text(ui_label_wifi_ssid_value, wifi_status.ssid);
    lv_label_set_text(ui_label_wifi_ip_value, wifi_status.ip_address);
    if (wifi_status.is_start) {
        lv_obj_set_style_bg_color(ui_button_stop_wifi, lv_color_hex(0x4DDD66), 0);
        lv_label_set_text(ui_label_stop_wifi, "Stop Wi-Fi");
        lv_obj_clear_state(ui_button_change_wifi, LV_STATE_DISABLED);
    } else {
        lv_obj_set_style_bg_color(ui_button_stop_wifi, lv_color_hex(0xA4DEA4), 0);
        lv_label_set_text(ui_label_stop_wifi, "Start Wi-Fi");
        lv_obj_add_state(ui_button_change_wifi, LV_STATE_DISABLED); // wifi关闭时禁用配网按钮
    }
}

static lv_timer_t *update_timer;
void settings2_page_didAppear(lv_obj_t *settings2_page) { update_timer = lv_timer_create(settings2_page_timer_cb, 500, NULL); }
void settings2_page_willDisappear(lv_obj_t *settings2_page) {
    nvs_save_config();
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    if (busy_msgbox) {
        lv_msgbox_close(busy_msgbox);
        busy_msgbox = NULL;
    }
    if (wifi_pairing_msgbox) {
        lv_msgbox_close(wifi_pairing_msgbox);
        wifi_pairing_msgbox = NULL;
    }
}
