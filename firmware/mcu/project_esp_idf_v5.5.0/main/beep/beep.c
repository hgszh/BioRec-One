#include "beep.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static void init_beep_pwm(void) {
    ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                      .duty_resolution = LEDC_TIMER_8_BIT,
                                      .timer_num = LEDC_TIMER_0,
                                      .freq_hz = 500,
                                      .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                          .channel = LEDC_CHANNEL_0,
                                          .timer_sel = LEDC_TIMER_0,
                                          .intr_type = LEDC_INTR_DISABLE,
                                          .gpio_num = BEEP_IO,
                                          .duty = 0,
                                          .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

static void beep_start(uint16_t freq, uint8_t duty) {
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void beep_stop(void) { ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0); }
#define PACK_BEEP_CMD(freq, duty, time_ms)                                                                                       \
    (((uint32_t)(freq & 0x3FFF) << 18) | ((uint32_t)(duty & 0xFF) << 10) | ((uint32_t)(time_ms & 0x3FF)))
#define BEEP_CMD_GET_FREQ(cmd) (((cmd) >> 18) & 0x3FFF)
#define BEEP_CMD_GET_DUTY(cmd) (((cmd) >> 10) & 0xFF)
#define BEEP_CMD_GET_DURATION(cmd) ((cmd) & 0x3FF)

static void beep_task(void *arg) {
    init_beep_pwm();
    beep_stop();
    uint32_t cmd;
    while (1) {
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &cmd, portMAX_DELAY) == pdTRUE) {
            uint32_t freq = BEEP_CMD_GET_FREQ(cmd);
            uint8_t duty = BEEP_CMD_GET_DUTY(cmd);
            uint32_t time = BEEP_CMD_GET_DURATION(cmd);
            if (freq == 0 || time == 0) beep_stop();
            else {
                beep_start(freq, duty);
                vTaskDelay(pdMS_TO_TICKS(time));
                beep_stop();
            }
        }
    }
}

static TaskHandle_t beep_task_handle = NULL;
void init_beep_task(void) { xTaskCreate(beep_task, "beep", 2048, NULL, tskIDLE_PRIORITY + 1, &beep_task_handle); }

void send_buzzer_cmd(uint16_t freq, uint8_t duty, uint16_t duration_ms) {
    uint32_t cmd = PACK_BEEP_CMD(freq, duty, duration_ms);
    xTaskNotify(beep_task_handle, cmd, eSetValueWithOverwrite);
}

typedef struct {
    volatile bool enable_startup_beep;
    volatile bool enable_btn_beep;
    volatile bool enable_r_wave_beep;
    volatile bool enable_ntp_beep;
    volatile bool enable_countdown_beep;
} beep_config_t;
static beep_config_t beep_config = {.enable_startup_beep = true, .enable_btn_beep = true, .enable_r_wave_beep = true};

void poweron_beep_melody(void) {
    if (!beep_config.enable_startup_beep) return;
    send_buzzer_cmd(800, 128, 100);
    vTaskDelay(100);
    send_buzzer_cmd(1000, 128, 100);
    vTaskDelay(100);
    send_buzzer_cmd(1200, 128, 100);
}

void btn_beep(void) {
    if (!beep_config.enable_btn_beep) return;
    send_buzzer_cmd(4000, 10, 50);
}

void back_home_long_beep(void) {
    if (!beep_config.enable_btn_beep) return;
    send_buzzer_cmd(4000, 128, 180);
}

void ntp_sync_beep(void) {
    if (!beep_config.enable_ntp_beep) return;
    send_buzzer_cmd(2700, 10, 70);
}

void r_wave_beep(void) {
    if (!beep_config.enable_r_wave_beep) return;
    send_buzzer_cmd(7000, 30, 70);
}

void countdown_beep(void) {
    if (!beep_config.enable_countdown_beep) return;
    static bool flag = true;
    if (flag) {
        send_buzzer_cmd(2700, 50, 70);
        flag = false;
    } else flag = true;
}

void set_system_beep(bool en) {
    beep_config.enable_startup_beep = en;
    beep_config.enable_btn_beep = en;
    beep_config.enable_ntp_beep = en;
    beep_config.enable_countdown_beep = en;
}

void set_heartbeat_beep(bool en) { beep_config.enable_r_wave_beep = en; }

bool get_system_beep(void) { return (beep_config.enable_startup_beep && beep_config.enable_btn_beep); }
bool get_heartbeat_beep(void) { return beep_config.enable_r_wave_beep; }
