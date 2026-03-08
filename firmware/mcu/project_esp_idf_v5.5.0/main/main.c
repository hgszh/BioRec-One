#include "about_page.h"
#include "console_app.h"
#include "data_logger.h"
#include "main_page.h"
#include "mcu_fpga_comm.h"
#include "nvs_app.h"
#include "pm.h"
#include "rx8025t.h"
#include "settings1_page.h"
#include "settings2_page.h"
#include "settings3_page.h"
#include "sntp_app.h"
#include "ui_hal.h"
#include "wifi_app.h"

void app_main(void) {

    init_beep_task();
    init_fpga_comm_task();
    init_data_logger();
    init_nvs();

    load_and_apply_timezone();
    esp_err_t rtc_err = rx8025t_init();
    if (rtc_err == ESP_OK) {
        rx8025t_sync_to_system();
    }

    start_network_services(false);
    app_lcd_init();
    app_lvgl_init();

    if (lvgl_port_lock(0)) {
        lv_pm_init();

        lv_pm_page_t *page0 = lv_pm_create_page(0);
        page0->onLoad = main_page_init;
        page0->unLoad = main_page_destroy;
        page0->didAppear = main_page_didAppear;
        page0->willDisappear = main_page_willDisappear;

        lv_pm_page_t *page1 = lv_pm_create_page(1);
        page1->onLoad = settings1_page_init;
        page1->unLoad = settings1_page_destroy;
        page1->willDisappear = settings1_page_willDisappear;

        lv_pm_page_t *page2 = lv_pm_create_page(2);
        page2->onLoad = settings2_page_init;
        page2->unLoad = settings2_page_destroy;
        page2->didAppear = settings2_page_didAppear;
        page2->willDisappear = settings2_page_willDisappear;

        lv_pm_page_t *page3 = lv_pm_create_page(3);
        page3->onLoad = settings3_page_init;
        page3->unLoad = settings3_page_destroy;
        page3->willDisappear = settings3_page_willDisappear;

        lv_pm_page_t *page4 = lv_pm_create_page(4);
        page4->onLoad = about_page_init;
        page4->unLoad = about_page_destroy;

        lv_pm_open_options_t options = {.animation = LV_PM_ANIMA_POPUP};
        lv_pm_open_page(0, &options);
        lvgl_port_unlock();
    }

    lcd_backlight_on(80);
    poweron_beep_melody();

    vTaskDelay(5000);
    init_console();
}
