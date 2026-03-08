#include "main_page.h"
#include "data_logger.h"
#include "ecg_process.h"
#include "pm.h"
#include "sntp_app.h"
#include "ui_font_agencyb16.h"
#include "ui_font_agencyb17.h"
#include "ui_font_sharetech18.h"
#include "ui_font_sharetech52.h"
#include "ui_hal.h"
#include "ui_img_icon_countdown_png.h"
#include "ui_img_icon_heart_png.h"
#include "ui_img_icon_overload_png.h"
#include "ui_img_icon_pausing_png.h"
#include "ui_img_icon_recording_png.h"
#include "ui_img_icon_saving_png.h"
#include "ui_img_icon_wifi_png.h"
#include "wifi_app.h"

LV_IMG_DECLARE(ui_img_icon_heart_png);
LV_IMG_DECLARE(ui_img_icon_overload_png);
LV_IMG_DECLARE(ui_img_icon_wifi_png);
LV_IMG_DECLARE(ui_img_icon_countdown_png);
LV_IMG_DECLARE(ui_img_icon_saving_png);
LV_IMG_DECLARE(ui_img_icon_recording_png);
LV_IMG_DECLARE(ui_img_icon_pausing_png);
LV_FONT_DECLARE(ui_font_agencyb16);
LV_FONT_DECLARE(ui_font_agencyb17);
LV_FONT_DECLARE(ui_font_sharetech18);
LV_FONT_DECLARE(ui_font_sharetech52);

static lv_obj_t *ui_ecg_chart = NULL;
static lv_chart_series_t *ui_ecg_series_main = NULL;
static lv_obj_t *ui_label_hr_text = NULL;
static lv_obj_t *ui_icon_heart = NULL;
static lv_obj_t *ui_icon_wifi = NULL;
static lv_obj_t *ui_icon_overload = NULL;
static lv_obj_t *ui_label_time = NULL;
static lv_obj_t *ui_label_bandwidth = NULL;
static lv_obj_t *ui_label_notch_state = NULL;
static lv_obj_t *ui_label_rmssd = NULL;
static lv_obj_t *ui_label_sdnn = NULL;
static lv_obj_t *ui_label_hr_value = NULL;
static lv_obj_t *ui_mv_ref_line = NULL;
static lv_obj_t *ui_label_mv = NULL;
static lv_obj_t *ui_label_recording_time = NULL;
static lv_obj_t *ui_icon_record_status = NULL;
static lv_obj_t *ui_label_paperspeed = NULL;
static HRV_Calculator hrv_calc;

/*************************************************************************************************************/
static void heart_anim_cb(void *obj, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)obj, v, 0); }
static void heart_scale_cb(void *obj, int32_t v) { lv_img_set_zoom((lv_obj_t *)obj, v); }
void trigger_heart_flash(lv_obj_t *icon) {
    lv_anim_t anim1;
    lv_anim_init(&anim1);
    lv_anim_set_var(&anim1, icon);
    lv_anim_set_exec_cb(&anim1, heart_anim_cb);
    lv_anim_set_time(&anim1, 100);
    lv_anim_set_values(&anim1, LV_OPA_TRANSP + 40, LV_OPA_COVER);
    lv_anim_set_playback_time(&anim1, 300);
    lv_anim_set_playback_delay(&anim1, 200);
    lv_anim_set_repeat_count(&anim1, 0);
    lv_anim_set_path_cb(&anim1, lv_anim_path_ease_in_out);
    lv_anim_t anim2;
    lv_anim_init(&anim2);
    lv_anim_set_var(&anim2, icon);
    lv_anim_set_exec_cb(&anim2, heart_scale_cb);
    lv_anim_set_time(&anim2, 100);
    lv_anim_set_values(&anim2, 256, 305);
    lv_anim_set_playback_time(&anim2, 300);
    lv_anim_set_playback_delay(&anim2, 200);
    lv_anim_set_repeat_count(&anim2, 0);
    lv_anim_set_path_cb(&anim2, lv_anim_path_ease_in_out);
    lv_anim_start(&anim1);
    lv_anim_start(&anim2);
}

/*************************************************************************************************************/
static void update_ecg_chart(void) {
    int8_t data_batch[32];
    uint8_t cnt = xStreamBufferReceive(ecg_chart_stream_buffer, data_batch, sizeof(data_batch), 0);
    if (cnt == 0) return;
    for (size_t i = 0; i < cnt - 1; i++) {
        lv_chart_set_next_value(ui_ecg_chart, ui_ecg_series_main, data_batch[i], 0);
    }
    lv_chart_set_next_value(ui_ecg_chart, ui_ecg_series_main, data_batch[cnt - 1], 20);
    lv_chart_refresh(ui_ecg_chart);
}
static const uint16_t SPEED_TO_POINTS_MAP[ECG_SPEED_COUNT] = {640, 320, 160};
static ecg_paper_speed_t g_current_ecg_speed = ECG_SPEED_25_MM_S;
void set_ecg_paper_speed(ecg_paper_speed_t speed) {
    if (speed < ECG_SPEED_COUNT) g_current_ecg_speed = speed;
}
ecg_paper_speed_t get_ecg_paper_speed(void) { return g_current_ecg_speed; }
uint16_t get_ecg_chart_point_count(void) { return SPEED_TO_POINTS_MAP[g_current_ecg_speed]; }

/*************************************************************************************************************/
static lv_obj_t *prepare_countdown_msgbox = NULL;
static lv_obj_t *finish_countdown_msgbox = NULL;
static lv_obj_t *busy_msgbox = NULL;
static lv_obj_t *card_error_msgbox = NULL;
static uint32_t msgbox_countdown_ticks;
static lv_timer_t *msgbox_timer;
static void handel_msgbox(lv_timer_t *timer) {
    if (prepare_countdown_msgbox) {
        if (msgbox_countdown_ticks > 0) {
            msgbox_countdown_ticks--;
            lv_obj_t *content_label = lv_msgbox_get_text(prepare_countdown_msgbox);
            lv_label_set_text_fmt(content_label, "Starting in ...  (%lds)\n------------------\nPress any key to skip countdown.", (msgbox_countdown_ticks + 1) / 2);
            countdown_beep();
        } else {
            lv_msgbox_close(prepare_countdown_msgbox);
            prepare_countdown_msgbox = NULL;
            lv_timer_del(msgbox_timer);
            msgbox_timer = NULL;
            start_data_logger();
        }
    }
    if (finish_countdown_msgbox) {
        if (msgbox_countdown_ticks > 0) {
            msgbox_countdown_ticks--;
            lv_obj_t *content_label = lv_msgbox_get_text(finish_countdown_msgbox);
            lv_label_set_text_fmt(content_label, "Finalizing file. Please wait ...   (%lds)", (msgbox_countdown_ticks + 1) / 2);
        } else {
            lv_msgbox_close(finish_countdown_msgbox);
            finish_countdown_msgbox = NULL;
            lv_timer_del(msgbox_timer);
            msgbox_timer = NULL;
            lv_obj_add_flag(ui_icon_record_status, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_label_recording_time, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (busy_msgbox) {
        if (msgbox_countdown_ticks > 0) {
            msgbox_countdown_ticks--;
            lv_obj_t *content_label = lv_msgbox_get_text(busy_msgbox);
            lv_label_set_text_fmt(content_label, "Resource is busy     (%lds)\n------------------\nPress any key to close.", (msgbox_countdown_ticks + 1) / 2);
        } else {
            lv_msgbox_close(busy_msgbox);
            busy_msgbox = NULL;
            lv_timer_del(msgbox_timer);
            msgbox_timer = NULL;
        }
    }
    if (card_error_msgbox) {
        if (msgbox_countdown_ticks > 0) {
            msgbox_countdown_ticks--;
        } else {
            lv_msgbox_close(card_error_msgbox);
            card_error_msgbox = NULL;
            lv_timer_del(msgbox_timer);
            msgbox_timer = NULL;
        }
    }
}
static data_format_t file_format_for_msgbox = DATA_FORMAT_CSV;
static void start_recording_with_countdown(void) {
    if (prepare_countdown_msgbox) return;
    data_logger_t data_logger_status;
    data_logger_get_status_copy(&data_logger_status);
    if (data_logger_status.prepare_countdown_sec == 0) {
        start_data_logger();
        return;
    }
    msgbox_countdown_ticks = data_logger_status.prepare_countdown_sec * 2;
    char str1[128];
    snprintf(str1, sizeof(str1), "Starting in ...  (%lds)\n------------------\nPress any key to skip countdown.", (msgbox_countdown_ticks + 1) / 2);
    file_format_for_msgbox = data_logger_status.selected_format;
    if (file_format_for_msgbox == DATA_FORMAT_BDF) {
        prepare_countdown_msgbox = lv_msgbox_create(NULL, "Prepare for Recording [BDF]", str1, NULL, false);
    } else {
        prepare_countdown_msgbox = lv_msgbox_create(NULL, "Prepare for Recording [CSV]", str1, NULL, false);
    }
    lv_obj_t *title_label = lv_msgbox_get_title(prepare_countdown_msgbox);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x000000), 0);
    lv_obj_t *content_label = lv_msgbox_get_text(prepare_countdown_msgbox);
    lv_obj_set_style_text_font(content_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(content_label, lv_color_hex(0x000000), 0);
    lv_obj_set_width(content_label, 153);
    lv_obj_t *content_area = lv_msgbox_get_content(prepare_countdown_msgbox);
    lv_obj_set_height(content_area, 70);
    lv_obj_set_layout(content_area, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content_area, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(content_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(content_area, 25, 0);
    lv_obj_t *img = lv_img_create(content_area);
    lv_img_set_src(img, &ui_img_icon_countdown_png);
    lv_img_set_zoom(img, 200);
    lv_obj_center(prepare_countdown_msgbox);
    msgbox_timer = lv_timer_create(handel_msgbox, 500, NULL);
}
static void stop_recording_with_countdown(void) {
    if (finish_countdown_msgbox) return;
    stop_data_logger();
    msgbox_countdown_ticks = 3 * 2;
    char buf[128];
    snprintf(buf, sizeof(buf), "Finalizing file. Please wait ...   (%lds)", (msgbox_countdown_ticks + 1) / 2);
    if (file_format_for_msgbox == DATA_FORMAT_BDF) {
        finish_countdown_msgbox = lv_msgbox_create(NULL, "Saving Recording [BDF]", buf, NULL, false);
    } else {
        finish_countdown_msgbox = lv_msgbox_create(NULL, "Saving Recording [CSV]", buf, NULL, false);
    }
    lv_obj_t *title_label = lv_msgbox_get_title(finish_countdown_msgbox);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x000000), 0);
    lv_obj_t *content_label = lv_msgbox_get_text(finish_countdown_msgbox);
    lv_obj_set_style_text_font(content_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(content_label, lv_color_hex(0x000000), 0);
    lv_obj_set_width(content_label, 153);
    lv_obj_t *content_area = lv_msgbox_get_content(finish_countdown_msgbox);
    lv_obj_set_height(content_area, 40);
    lv_obj_set_layout(content_area, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content_area, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(content_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(content_area, 20, 0);
    lv_obj_t *img = lv_img_create(content_area);
    lv_img_set_src(img, &ui_img_icon_saving_png);
    lv_img_set_zoom(img, 150);
    lv_obj_center(finish_countdown_msgbox);
    msgbox_timer = lv_timer_create(handel_msgbox, 500, NULL);
}
static void show_busy_msgbox(void) {
    if (busy_msgbox) return;
    msgbox_countdown_ticks = 3 * 2;
    char buf[128];
    snprintf(buf, sizeof(buf), "Resource is busy     (%lds)\n------------------\nPress any key to close.", (msgbox_countdown_ticks + 1) / 2);
    busy_msgbox = lv_msgbox_create(NULL, "Notice", buf, NULL, false);
    lv_obj_t *title_label = lv_msgbox_get_title(busy_msgbox);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x000000), 0);
    lv_obj_t *content_label = lv_msgbox_get_text(busy_msgbox);
    lv_obj_set_style_text_font(content_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(content_label, lv_color_hex(0x000000), 0);
    lv_obj_center(busy_msgbox);
    msgbox_timer = lv_timer_create(handel_msgbox, 500, NULL);
}
static void show_card_error_msgbox(data_logger_error_t err) {
    if (card_error_msgbox) return;
    msgbox_countdown_ticks = 0xffffffff;
    const char *msg = NULL;
    if (err == LOGGER_ERROR_SD_INIT_FAIL) {
        msg = "Card init failed.\nPlease check the card and reboot.\n------------------\nPress any key to close.";
    } else if (err == LOGGER_ERROR_FILE_OPEN) {
        msg = "File open failed.\nPlease check the card and reboot.\n------------------\nPress any key to close.";
    } else if (err == LOGGER_ERROR_FILE_WRITE) {
        msg = "File write failed.\nPlease check the card and reboot.\n------------------\nPress any key to close.";
    } else if (err == LOGGER_ERROR_SD_FULL) {
        msg = "Card full.\nPlease check the card and reboot.\n------------------\nPress any key to close.";
    } else {
        msg = "Card error.\nPlease check the card and reboot.\n------------------\nPress any key to close.";
    }
    card_error_msgbox = lv_msgbox_create(NULL, "Warning", msg, NULL, false);
    lv_obj_t *title_label = lv_msgbox_get_title(card_error_msgbox);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x000000), 0);
    lv_obj_t *content_label = lv_msgbox_get_text(card_error_msgbox);
    lv_obj_set_style_text_font(content_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(content_label, lv_color_hex(0x000000), 0);
    lv_obj_center(card_error_msgbox);
    msgbox_timer = lv_timer_create(handel_msgbox, 100, NULL);
}
// 允许用户在对话框激活期间，通过再次点触按键提前退出倒计时。
// 常规输入回调(indev_cb)被对话框阻塞，因此需要在此处手动轮询检测按键事件
static void handle_msgbox_early_exit(void) {
    static uint16_t last_msgbox_countdown_ticks = 0;
    static int8_t last_id = -1;
    static uint8_t trigger_cnt = 0;
    if (msgbox_countdown_ticks > 0 && last_msgbox_countdown_ticks == 0) { // 检测对话框是否刚刚启动
        trigger_cnt = 0;
        last_id = button_get_pressed_id(); // 立即获取当前按键状态，以防用户启动后还未松手
    }
    if (msgbox_countdown_ticks > 0) {
        int8_t key_id = button_get_pressed_id();
        if ((last_id != -1) && (key_id == -1)) { // 检测松手事件
            trigger_cnt++;
            // 需要两次松手事件,第一次是启动长按的松手，第二次是取消操作的松手
            if (trigger_cnt >= 2) {
                trigger_cnt = 0;
                msgbox_countdown_ticks = 0;
            }
            // 如果是SD卡错误对话框，只需一次松手
            if (card_error_msgbox) {
                msgbox_countdown_ticks = 0;
            }
        }
        last_id = key_id;
    }
    last_msgbox_countdown_ticks = msgbox_countdown_ticks;
}

/*************************************************************************************************************/
static void format_recording_time_hms(uint32_t total_seconds, char *buffer, size_t buffer_size) {
    unsigned int hours = total_seconds / 3600;
    unsigned int minutes = (total_seconds % 3600) / 60;
    unsigned int seconds = total_seconds % 60;
    if (hours > 999) {
        hours = 999;
        minutes = 59;
        seconds = 59;
    }
    snprintf(buffer, buffer_size, "%02u:%02u:%02u", hours, minutes, seconds);
}

static bool has_recording_finish_msgbox_shown = true;
static void update_recording_status_ui(void) {
    char time_str_buf[24];
    data_logger_t data_logger_status;
    data_logger_get_status_copy(&data_logger_status);
    data_logger_error_t err = data_logger_status.last_error;
    if (err != LOGGER_ERROR_NONE) {
        if (err == LOGGER_ERROR_SD_BUSY) show_busy_msgbox();
        else show_card_error_msgbox(err);
        clear_data_logger_error();
    }
    if (data_logger_status.is_recording && !data_logger_status.is_paused) {
        lv_obj_clear_flag(ui_icon_record_status, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_label_recording_time, LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(ui_icon_record_status, &ui_img_icon_recording_png);
        format_recording_time_hms(data_logger_status.elapsed_recording_time_sec, time_str_buf, sizeof(time_str_buf));
        lv_label_set_text(ui_label_recording_time, time_str_buf);
        has_recording_finish_msgbox_shown = false;
    } else if (data_logger_status.is_recording && data_logger_status.is_paused) {
        lv_img_set_src(ui_icon_record_status, &ui_img_icon_pausing_png);
    } else if (!data_logger_status.is_recording && !has_recording_finish_msgbox_shown) {
        stop_recording_with_countdown();          // 定时自动停止的情况下，仍要显示提示框
        has_recording_finish_msgbox_shown = true; // 确保只触发一次
    }
}

/*************************************************************************************************************/
static void indev_cb(lv_event_t *e) {
    uint8_t btn_id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        switch (btn_id) {
        case 0:
            hrv_clear_window(&hrv_calc);
            lv_label_set_text(ui_label_sdnn, "SDNN: 00 ms");
            lv_label_set_text(ui_label_rmssd, "RMSSD: 00 ms");
            btn_beep();
            break;
        case 2:
            btn_beep();
            lv_pm_open_options_t options = {.animation = LV_PM_ANIMA_SLIDE_LEFT, .target = LV_PM_TARGET_SELF};
            lv_pm_open_page(1, &options);
            break;
        case 3:
            if (atomic_load(&g_data_logger.is_recording) && !atomic_load(&g_data_logger.is_paused)) {
                pause_data_logger();
                btn_beep();
            } else if (atomic_load(&g_data_logger.is_recording) && atomic_load(&g_data_logger.is_paused)) {
                resume_data_logger();
                btn_beep();
            }
            break;
        default: break;
        }
    }
    if (code == LV_EVENT_LONG_PRESSED) {
        switch (btn_id) {
        case 3:
            data_logger_t data_logger_status;
            data_logger_get_status_copy(&data_logger_status);
            if (data_logger_status.is_sdcard_present) {
                if (!data_logger_status.is_recording) {
                    start_recording_with_countdown();
                } else {
                    stop_recording_with_countdown();
                    has_recording_finish_msgbox_shown = true;
                }
                back_home_long_beep();
            }
            break;
        case 1:
            back_home_long_beep();
            lv_pm_open_options_t options = {.animation = LV_PM_ANIMA_NONE, .target = LV_PM_TARGET_SELF};
            lv_pm_open_page(4, &options);
            break;
        default: break;
        }
    }
}

/*************************************************************************************************************/
void main_page_timer_cb(lv_timer_t *timer) {
    msg_t recv;
    uint16_t sdnn_val = 0;
    uint16_t rmssd_val = 0;
    static int time_update_counter = 0;
    if (++time_update_counter >= 10) { // 每100ms更新一次
        time_update_counter = 0;
        // 更新时间
        struct tm timeinfo;
        sntp_get_timeinfo(&timeinfo);
        char time_str[32];
        strftime(time_str, sizeof(time_str), " %Y-%m-%d  %H:%M:%S", &timeinfo);
        lv_label_set_text(ui_label_time, time_str);
        // 更新WiFi图标
        if (wifi_status.is_valid) lv_obj_set_style_opa(ui_icon_wifi, 210, 0);
        else lv_obj_set_style_opa(ui_icon_wifi, 20, 0);
        // 更新录制状态
        update_recording_status_ui();
    }
    if (xQueueReceive(event_queue, &recv, 0) == pdTRUE) {
        switch (recv.type) {
        case EVT_R_WAVE_DET:
            trigger_heart_flash(ui_icon_heart);
            r_wave_beep();
            break;
        case EVT_OVERLOAD:
            if (recv.is_ovld) lv_obj_set_style_opa(ui_icon_overload, LV_OPA_COVER, 0);
            else lv_obj_set_style_opa(ui_icon_overload, 20, 0);
            break;
        case EVT_HEARTRATE:
            // 更新心率
            static char hr_str[8];
            sprintf(hr_str, "%02u", recv.hr);
            lv_label_set_text(ui_label_hr_value, hr_str);
            // 更新HRV
            if (recv.rr_ms > 0) {
                hrv_add_rr(&hrv_calc, recv.rr_ms);
                hrv_calculate(&hrv_calc, &sdnn_val, &rmssd_val);
            } else {
                hrv_clear_window(&hrv_calc);
                sdnn_val = 0;
                rmssd_val = 0;
            }
            char buf[16];
            sprintf(buf, "SDNN: %02u ms", (sdnn_val > 300) ? 300 : sdnn_val);
            lv_label_set_text(ui_label_sdnn, buf);
            sprintf(buf, "RMSSD: %02u ms", (rmssd_val > 300) ? 300 : rmssd_val);
            lv_label_set_text(ui_label_rmssd, buf);
            break;
        case EVT_REF_LINE: lv_obj_set_height(ui_mv_ref_line, recv.mv_ref_lenth); break;
        }
    }
    update_ecg_chart();
    handle_msgbox_early_exit();
}

/*************************************************************************************************************/
void main_page_init(lv_obj_t *main_page) {
    create_vitrual_btn(main_page, indev_cb);
    lv_obj_set_style_bg_color(main_page, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(main_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_indev_enable(NULL, false); // 禁用输入，避免按键切换页面时的亚稳态

    ui_ecg_chart = lv_chart_create(main_page);
    lv_obj_set_size(ui_ecg_chart, 320, 170);
    lv_obj_set_align(ui_ecg_chart, LV_ALIGN_BOTTOM_MID);
    lv_chart_set_type(ui_ecg_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ui_ecg_chart, get_ecg_chart_point_count());
    lv_chart_set_range(ui_ecg_chart, LV_CHART_AXIS_PRIMARY_Y, -128, 127);
    lv_chart_set_div_line_count(ui_ecg_chart, 0, 0);
    ui_ecg_series_main = lv_chart_add_series(ui_ecg_chart, lv_color_hex(0x33FF33), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_update_mode(ui_ecg_chart, LV_CHART_UPDATE_MODE_CIRCULAR);
    lv_obj_set_style_radius(ui_ecg_chart, 0, 0);
    lv_obj_set_style_bg_color(ui_ecg_chart, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_side(ui_ecg_chart, LV_BORDER_SIDE_NONE, 0);
    lv_obj_set_style_pad_all(ui_ecg_chart, 0, 0);
    lv_obj_set_style_size(ui_ecg_chart, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(ui_ecg_chart, 2, LV_PART_ITEMS | LV_STATE_DEFAULT);

    ui_label_hr_text = lv_label_create(main_page);
    lv_obj_set_pos(ui_label_hr_text, 269, 3);
    lv_label_set_text(ui_label_hr_text, "HR");
    lv_obj_set_style_text_color(ui_label_hr_text, lv_color_hex(0x14DC3C), 0);
    lv_obj_set_style_text_letter_space(ui_label_hr_text, 1, 0);
    lv_obj_set_style_text_align(ui_label_hr_text, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(ui_label_hr_text, &ui_font_agencyb17, 0);

    ui_icon_heart = lv_img_create(main_page);
    lv_img_set_src(ui_icon_heart, &ui_img_icon_heart_png);
    lv_obj_set_pos(ui_icon_heart, 248, 6);
    lv_obj_set_style_opa(ui_icon_heart, 40, 0);

    ui_icon_overload = lv_img_create(main_page);
    lv_img_set_src(ui_icon_overload, &ui_img_icon_overload_png);
    lv_obj_set_pos(ui_icon_overload, 42, -106);
    lv_obj_set_align(ui_icon_overload, LV_ALIGN_CENTER);
    lv_obj_set_style_opa(ui_icon_overload, 20, 0);

    ui_label_time = lv_label_create(main_page);
    lv_obj_set_pos(ui_label_time, 10, 4);
    lv_label_set_text(ui_label_time, " 1970-01-01  00:00:00");
    lv_obj_set_style_text_color(ui_label_time, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_letter_space(ui_label_time, 1, 0);
    lv_obj_set_style_text_align(ui_label_time, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(ui_label_time, &ui_font_agencyb16, 0);

    ui_icon_wifi = lv_img_create(main_page);
    lv_img_set_src(ui_icon_wifi, &ui_img_icon_wifi_png);
    lv_obj_set_pos(ui_icon_wifi, 20, -106);
    lv_obj_set_align(ui_icon_wifi, LV_ALIGN_CENTER);
    lv_obj_set_style_opa(ui_icon_wifi, 0, 0);
    lv_obj_set_style_img_recolor(ui_icon_wifi, lv_color_hex(0x000000), 0);
    lv_obj_set_style_img_recolor_opa(ui_icon_wifi, 20, 0);

    ui_label_bandwidth = lv_label_create(main_page);
    lv_obj_set_pos(ui_label_bandwidth, 19, 27);
    const int fh_map[] = {40, 100, 200};
    int lpf_index = comm_get_lpf();
    int fh = fh_map[lpf_index];
    float fl_afe = comm_get_afe_hpf() ? 0.05f : 1.5f;
    float fl_dsp = comm_get_dsp_hpf() ? 0.05f : 0.67f;
    float fl = (fl_afe > fl_dsp) ? fl_afe : fl_dsp;
    char bandwidth_str[32];
    sprintf(bandwidth_str, "BW: %.2f~%d Hz", fl, fh);
    lv_label_set_text(ui_label_bandwidth, bandwidth_str);
    lv_obj_set_style_text_color(ui_label_bandwidth, lv_color_hex(0x14DC3C), 0);
    lv_obj_set_style_text_align(ui_label_bandwidth, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(ui_label_bandwidth, &ui_font_agencyb17, 0);

    ui_label_notch_state = lv_label_create(main_page);
    lv_obj_set_pos(ui_label_notch_state, 19, 48);
    char *notch_str;
    if (comm_get_notch()) notch_str = "Notch: ON";
    else notch_str = "Notch: OFF";
    lv_label_set_text(ui_label_notch_state, notch_str);
    lv_obj_set_style_text_color(ui_label_notch_state, lv_color_hex(0x14DC3C), 0);
    lv_obj_set_style_text_align(ui_label_notch_state, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(ui_label_notch_state, &ui_font_agencyb17, 0);

    ui_label_rmssd = lv_label_create(main_page);
    lv_obj_set_pos(ui_label_rmssd, 132, 48);
    lv_label_set_text(ui_label_rmssd, "RMSSD: 00 ms");
    lv_obj_set_style_text_color(ui_label_rmssd, lv_color_hex(0x14DC3C), 0);
    lv_obj_set_style_text_align(ui_label_rmssd, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(ui_label_rmssd, &ui_font_agencyb17, 0);

    ui_label_sdnn = lv_label_create(main_page);
    lv_obj_set_pos(ui_label_sdnn, 141, 27);
    lv_label_set_text(ui_label_sdnn, "SDNN: 00 ms");
    lv_obj_set_style_text_color(ui_label_sdnn, lv_color_hex(0x14DC3C), 0);
    lv_obj_set_style_text_align(ui_label_sdnn, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(ui_label_sdnn, &ui_font_agencyb17, 0);

    ui_label_hr_value = lv_label_create(main_page);
    lv_obj_set_pos(ui_label_hr_value, 114, -70);
    lv_obj_set_align(ui_label_hr_value, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_hr_value, "00");
    lv_obj_set_style_text_color(ui_label_hr_value, lv_color_hex(0x14DC3C), 0);
    lv_obj_set_style_text_letter_space(ui_label_hr_value, -4, 0);
    lv_obj_set_style_text_font(ui_label_hr_value, &ui_font_sharetech52, 0);

    ui_mv_ref_line = lv_obj_create(main_page);
    lv_obj_set_width(ui_mv_ref_line, 2);
    lv_obj_set_height(ui_mv_ref_line, 100);
    lv_obj_set_pos(ui_mv_ref_line, -141, 34);
    lv_obj_set_align(ui_mv_ref_line, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_mv_ref_line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_color(ui_mv_ref_line, lv_color_hex(0xA0522D), 0);
    lv_obj_set_style_outline_width(ui_mv_ref_line, 5, 0);

    ui_label_mv = lv_label_create(main_page);
    lv_obj_set_pos(ui_label_mv, -115, 83);
    lv_obj_set_align(ui_label_mv, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_mv, "1mV");
    lv_obj_set_style_text_color(ui_label_mv, lv_color_hex(0xD2691E), 0);
    lv_obj_set_style_text_font(ui_label_mv, &ui_font_sharetech18, 0);
    lv_obj_set_style_bg_color(ui_label_mv, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_label_mv, 180, 0);

    ui_label_recording_time = lv_label_create(main_page);
    lv_obj_set_pos(ui_label_recording_time, 118, 105);
    lv_obj_set_align(ui_label_recording_time, LV_ALIGN_CENTER);
    lv_label_set_text(ui_label_recording_time, "00:00:00");
    lv_obj_set_style_text_color(ui_label_recording_time, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_letter_space(ui_label_recording_time, -1, 0);
    lv_obj_set_style_text_font(ui_label_recording_time, &ui_font_sharetech18, 0);
    lv_obj_set_style_bg_color(ui_label_recording_time, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_label_recording_time, 180, 0);
    lv_obj_add_flag(ui_label_recording_time, LV_OBJ_FLAG_HIDDEN);

    ui_icon_record_status = lv_img_create(main_page);
    lv_img_set_src(ui_icon_record_status, &ui_img_icon_pausing_png);
    lv_obj_set_pos(ui_icon_record_status, 65, 104);
    lv_obj_set_align(ui_icon_record_status, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_icon_record_status, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_icon_record_status, 220, 0);
    lv_obj_add_flag(ui_icon_record_status, LV_OBJ_FLAG_HIDDEN);

    ui_label_paperspeed = lv_label_create(main_page);
    lv_obj_set_pos(ui_label_paperspeed, -5, 195);
    lv_obj_set_align(ui_label_paperspeed, LV_ALIGN_TOP_RIGHT);
    ecg_paper_speed_t speed = get_ecg_paper_speed();
    switch (speed) {
    case ECG_SPEED_12_5_MM_S: lv_label_set_text(ui_label_paperspeed, "12.5mm/s"); break;
    case ECG_SPEED_25_MM_S: lv_label_set_text(ui_label_paperspeed, "25mm/s"); break;
    case ECG_SPEED_50_MM_S: lv_label_set_text(ui_label_paperspeed, "50mm/s"); break;
    default: lv_label_set_text(ui_label_paperspeed, "25mm/s"); break;
    }
    lv_obj_set_style_text_color(ui_label_paperspeed, lv_color_hex(0xD2691E), 0);
    lv_obj_set_style_text_font(ui_label_paperspeed, &ui_font_sharetech18, 0);
    lv_obj_set_style_bg_color(ui_label_paperspeed, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_label_paperspeed, 180, 0);
}

void main_page_destroy(lv_obj_t *main_page) {
    (void)main_page;
    ui_ecg_chart = NULL;
    ui_label_hr_text = NULL;
    ui_icon_heart = NULL;
    ui_label_time = NULL;
    ui_label_bandwidth = NULL;
    ui_label_notch_state = NULL;
    ui_label_rmssd = NULL;
    ui_label_sdnn = NULL;
    ui_label_hr_value = NULL;
    ui_mv_ref_line = NULL;
    ui_label_mv = NULL;
    ui_label_recording_time = NULL;
    ui_icon_record_status = NULL;
    ui_label_paperspeed = NULL;
    destroy_vitrual_btn();
}

static lv_timer_t *update_timer;
void main_page_didAppear(lv_obj_t *main_page) {
    update_timer = lv_timer_create(main_page_timer_cb, 10, NULL);
    hrv_clear_window(&hrv_calc);
    xStreamBufferReset(ecg_chart_stream_buffer);
    lv_indev_enable(NULL, true); // 待到页面稳定之后，启动输入
}

void main_page_willDisappear(lv_obj_t *main_page) {
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    if (msgbox_timer) {
        lv_timer_del(msgbox_timer);
        msgbox_timer = NULL;
    }
    if (prepare_countdown_msgbox) {
        lv_msgbox_close(prepare_countdown_msgbox);
        prepare_countdown_msgbox = NULL;
    }
    if (finish_countdown_msgbox) {
        lv_msgbox_close(finish_countdown_msgbox);
        finish_countdown_msgbox = NULL;
    }
    if (busy_msgbox) {
        lv_msgbox_close(busy_msgbox);
        busy_msgbox = NULL;
    }
    if (card_error_msgbox) {
        lv_msgbox_close(card_error_msgbox);
        card_error_msgbox = NULL;
    }
}
