#ifndef UI_HAL_H
#define UI_HAL_H

#include "beep.h"
#include "esp_err.h"
#include "esp_lvgl_port.h"

/* LCD size */
#define LCD_H_RES (240)
#define LCD_V_RES (320)

/* LCD settings */
#define LCD_SPI_NUM (SPI3_HOST)
#define LCD_PIXEL_CLK_HZ (80 * 1000 * 1000)
#define LCD_CMD_BITS (8)
#define LCD_PARAM_BITS (8)
#define LCD_COLOR_SPACE (ESP_LCD_COLOR_SPACE_RGB)
#define LCD_BITS_PER_PIXEL (16)
#define LCD_DRAW_BUFF_DOUBLE (1)
#define LCD_DRAW_BUFF_HEIGHT (30)
#define LCD_BL_ON_LEVEL (1)

/* LCD pins */
#define LCD_GPIO_SCLK (GPIO_NUM_11)
#define LCD_GPIO_MOSI (GPIO_NUM_10)
#define LCD_GPIO_RST (GPIO_NUM_9)
#define LCD_GPIO_DC (GPIO_NUM_46)
#define LCD_GPIO_CS (GPIO_NUM_3)
#define LCD_GPIO_BL (GPIO_NUM_20)
//#define LCD_GPIO_TE (GPIO_NUM_17)

/* AD key threshold */
#define AD_KEY1_THRESHOLD (300)
#define AD_KEY2_THRESHOLD (1115)
#define AD_KEY3_THRESHOLD (2269)
#define AD_KEY4_THRESHOLD (0)
#define AD_KEY_IDLE_THRESHOLD (2600)
#define AD_KEY_TOLERANCE (90)

esp_err_t app_lcd_init(void);
esp_err_t app_lvgl_init(void);
void lcd_backlight_on(uint16_t startup_delay_ms);
int8_t button_get_pressed_id(void);
extern lv_indev_t *indev_button;
void create_vitrual_btn(lv_obj_t *page, lv_event_cb_t btn_cb);
void destroy_vitrual_btn(void);

#endif