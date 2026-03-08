#include "ui_hal.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

static const char *TAG = "ui_hal";
/* LCD IO and panel */
static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_panel_handle_t lcd_panel = NULL;
/* LVGL display */
static lv_display_t *lvgl_disp = NULL;
esp_err_t app_lcd_init(void) {
    esp_err_t ret = ESP_OK;
    /* LCD backlight */
    gpio_config_t bk_gpio_config = {.mode = GPIO_MODE_OUTPUT, .pin_bit_mask = 1ULL << LCD_GPIO_BL};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(LCD_GPIO_BL, 0);
    /* LCD initialization */
    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_GPIO_SCLK,
        .mosi_io_num = LCD_GPIO_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * LCD_DRAW_BUFF_HEIGHT * sizeof(uint16_t),
        .isr_cpu_id = 1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");
    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_GPIO_DC,
        .cs_gpio_num = LCD_GPIO_CS,
        .pclk_hz = LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_NUM, &io_config, &lcd_io), err, TAG,
                      "New panel IO failed");
    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_GPIO_RST,
        .color_space = LCD_COLOR_SPACE,
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(lcd_io, &panel_config, &lcd_panel), err, TAG, "New panel failed");
    esp_lcd_panel_reset(lcd_panel);
    esp_lcd_panel_init(lcd_panel);
    esp_lcd_panel_disp_on_off(lcd_panel, true);
    esp_lcd_panel_invert_color(lcd_panel, true);
    /* LCD backlight on */
    // ESP_ERROR_CHECK(gpio_set_level(LCD_GPIO_BL, LCD_BL_ON_LEVEL));
    return ret;
err:
    if (lcd_panel) esp_lcd_panel_del(lcd_panel);
    if (lcd_io) esp_lcd_panel_io_del(lcd_io);
    spi_bus_free(LCD_SPI_NUM);
    return ret;
}

static void lv_port_indev_init(void);
esp_err_t app_lvgl_init(void) {
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,      /* LVGL task priority */
        .task_stack = 1024*6,      /* LVGL task stack size */
        .task_affinity = 1,      /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 50, /* Maximum sleep in LVGL task */
        .timer_period_ms = 5     /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");
    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {.io_handle = lcd_io,
                                              .panel_handle = lcd_panel,
                                              .buffer_size = LCD_H_RES * LCD_V_RES,
                                              .trans_size = LCD_H_RES * LCD_DRAW_BUFF_HEIGHT,
                                              .double_buffer = LCD_DRAW_BUFF_DOUBLE,
                                              .hres = LCD_H_RES,
                                              .vres = LCD_V_RES,
                                              .monochrome = false,
                                              .flags = {
                                                  .buff_spiram = true,
                                                  .buff_dma = false,
                                                  .sw_rotate = false,
                                              }};
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    lv_disp_set_rotation(lvgl_disp, LV_DISPLAY_ROTATION_270);
    lv_port_indev_init();
    return ESP_OK;
}

void lcd_backlight_on(uint16_t startup_delay_ms) {
    vTaskDelay(startup_delay_ms); // 延迟开背光，跳过初始化花屏
    gpio_set_level(LCD_GPIO_BL, LCD_BL_ON_LEVEL);
}

/*****************************************************************************************************************************/
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI("adc", "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) calibrated = true;
    }
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI("adc", "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) calibrated = true;
    }
#endif
    *out_handle = handle;
    if (ret == ESP_OK) ESP_LOGI("adc", "Calibration Success");
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) ESP_LOGW("adc", "eFuse not burnt, skip software calibration");
    else ESP_LOGE("adc", "Invalid arg or no memory");
    return calibrated;
}

static adc_cali_handle_t adc1_cali_handle = NULL;
static adc_oneshot_unit_handle_t adc1_handle;
static void init_ad_key(void) {
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_0, ADC_ATTEN_DB_12, &adc1_cali_handle);
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &config));
}

static int get_adc_volt(void) {
    int adc_raw, voltage;
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &adc_raw);
    adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage);
    return voltage;
}

int8_t button_get_pressed_id(void) {
    static int8_t hw_key = 0;
    int key_volt = get_adc_volt();
    if (key_volt < AD_KEY_IDLE_THRESHOLD) {
        if (abs(key_volt - AD_KEY1_THRESHOLD) < AD_KEY_TOLERANCE) hw_key = 0;
        else if (abs(key_volt - AD_KEY2_THRESHOLD) < AD_KEY_TOLERANCE) hw_key = 1;
        else if (abs(key_volt - AD_KEY3_THRESHOLD) < AD_KEY_TOLERANCE) hw_key = 2;
        else if (abs(key_volt - AD_KEY4_THRESHOLD) < AD_KEY_TOLERANCE) hw_key = 3;
        else hw_key = -1;
    } else hw_key = -1;
    return hw_key;
}

static void button_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    static uint8_t last_btn = 0;
    int8_t btn_act = button_get_pressed_id();
    if (btn_act >= 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        last_btn = btn_act;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->btn_id = last_btn;
}

static const lv_point_t btn_points[4] = {{1, 1}, {2, 1}, {3, 1}, {4, 1}};
static lv_obj_t *btn[4] = {NULL};
lv_indev_t *indev_button;
static void lv_port_indev_init(void) {
    init_ad_key();
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_BUTTON;
    indev_drv.read_cb = button_read;
    indev_drv.long_press_repeat_time = 200;
    indev_button = lv_indev_drv_register(&indev_drv);
    lv_indev_set_button_points(indev_button, btn_points);
}

void create_vitrual_btn(lv_obj_t *page, lv_event_cb_t btn_cb) {
    for (uint8_t i = 0; i < 4; i++) {
        btn[i] = lv_btn_create(page);
        lv_obj_set_size(btn[i], 1, 1);
        lv_obj_set_style_shadow_width(btn[i], 0, 0);
        // see https://forum.lvgl.io/t/missed-key-release-events-in-group/827/5
        lv_obj_set_style_bg_opa(btn[i], LV_OPA_TRANSP, 0);
        lv_obj_set_pos(btn[i], btn_points[i].x, btn_points[i].y);
        lv_obj_add_event_cb(btn[i], btn_cb, LV_EVENT_ALL, (void *)(intptr_t)i);
    }
}

void destroy_vitrual_btn(void) {
    for (uint8_t i = 0; i < 4; i++) btn[i] = NULL;
}
