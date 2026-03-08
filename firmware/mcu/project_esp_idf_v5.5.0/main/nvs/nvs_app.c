#include "nvs_app.h"
#include "data_logger.h"
#include "esp_log.h"
#include "main_page.h"
#include "mcu_fpga_comm.h"
#include "nvs_flash.h"
#include "settings2_page.h"
#include "ui_hal.h"
#include "wifi_app.h"

static nvs_handle_t sys_nvs_handle;
static const char *TAG = "nvs_app";

static void nvs_load_last_config(void) {
    uint8_t afe_hpf = 0;
    nvs_get_u8(sys_nvs_handle, "afe_hpf", &afe_hpf);
    comm_set_afe_hpf((afe_hpf_cutoff_t)afe_hpf);

    uint8_t dsp_hpf = 0;
    nvs_get_u8(sys_nvs_handle, "dsp_hpf", &dsp_hpf);
    comm_set_dsp_hpf((dsp_hpf_cutoff_t)dsp_hpf);

    uint8_t lpf = 0;
    nvs_get_u8(sys_nvs_handle, "lpf", &lpf);
    comm_set_lpf((lpf_cutoff_t)lpf);

    uint8_t notch = 0;
    nvs_get_u8(sys_nvs_handle, "notch", &notch);
    comm_set_notch((bool)notch);

    uint8_t sys_beep = 0;
    nvs_get_u8(sys_nvs_handle, "sys_beep", &sys_beep);
    set_system_beep((bool)sys_beep);

    uint8_t hr_beep = 0;
    nvs_get_u8(sys_nvs_handle, "hr_beep", &hr_beep);
    set_heartbeat_beep((bool)hr_beep);

    uint8_t paper_speed = 0;
    nvs_get_u8(sys_nvs_handle, "paper_speed", &paper_speed);
    set_ecg_paper_speed((ecg_paper_speed_t)paper_speed);

    uint8_t wifi_mode = 0;
    nvs_get_u8(sys_nvs_handle, "wifi_mode", &wifi_mode);
    set_wifi_mode((wifi_operating_mode_t)wifi_mode);

    uint8_t fs_auto = 0;
    nvs_get_u8(sys_nvs_handle, "fs_auto", &fs_auto);
    set_file_server_auto_start(fs_auto);
    settings2_page_set_autostart_setting(fs_auto);

    uint32_t rec_duration = 0;
    nvs_get_u32(sys_nvs_handle, "rec_duration", &rec_duration);
    data_logger_set_duration(rec_duration);

    uint8_t rec_countdown = 0;
    nvs_get_u8(sys_nvs_handle, "rec_cntdn", &rec_countdown);
    data_logger_set_countdown(rec_countdown);

    uint32_t rec_split_size = 0;
    nvs_get_u32(sys_nvs_handle, "rec_split", &rec_split_size);
    data_logger_set_split_size(rec_split_size);

    uint8_t rec_format = 0;
    nvs_get_u8(sys_nvs_handle, "rec_formt", &rec_format);
    data_logger_set_format((data_format_t)rec_format);
}

void init_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI("nvs", "Opening Non-Volatile Storage (NVS) handle...");

    ret = nvs_open("storage", NVS_READWRITE, &sys_nvs_handle);
    if (ret != ESP_OK) ESP_ERROR_CHECK(ret);
    else {
        ESP_LOGI("nvs", "Loading last configuration...");
    }
    uint8_t init_flag = 0;
    nvs_get_u8(sys_nvs_handle, "is_already_init", &init_flag);
    if (init_flag == 0xA6) {
        nvs_load_last_config();
    } else {
        nvs_save_config();
        save_timezone_to_nvs("CST-8");
        nvs_set_u8(sys_nvs_handle, "is_already_init", 0xA6);
        nvs_commit(sys_nvs_handle);
    }
}

void nvs_save_config(void) {
    uint8_t nvs_tmp;
    uint32_t nvs_tmp32;

    uint8_t afe_hpf = (uint8_t)comm_get_afe_hpf();
    nvs_get_u8(sys_nvs_handle, "afe_hpf", &nvs_tmp);
    if (nvs_tmp != afe_hpf) nvs_set_u8(sys_nvs_handle, "afe_hpf", afe_hpf);

    uint8_t dsp_hpf = (uint8_t)comm_get_dsp_hpf();
    nvs_get_u8(sys_nvs_handle, "dsp_hpf", &nvs_tmp);
    if (nvs_tmp != dsp_hpf) nvs_set_u8(sys_nvs_handle, "dsp_hpf", dsp_hpf);

    uint8_t lpf = (uint8_t)comm_get_lpf();
    nvs_get_u8(sys_nvs_handle, "lpf", &nvs_tmp);
    if (nvs_tmp != lpf) nvs_set_u8(sys_nvs_handle, "lpf", lpf);

    uint8_t notch = (uint8_t)comm_get_notch();
    nvs_get_u8(sys_nvs_handle, "notch", &nvs_tmp);
    if (nvs_tmp != notch) nvs_set_u8(sys_nvs_handle, "notch", notch);

    uint8_t sys_beep = (uint8_t)get_system_beep();
    nvs_get_u8(sys_nvs_handle, "sys_beep", &nvs_tmp);
    if (nvs_tmp != sys_beep) nvs_set_u8(sys_nvs_handle, "sys_beep", sys_beep);

    uint8_t hr_beep = (uint8_t)get_heartbeat_beep();
    nvs_get_u8(sys_nvs_handle, "hr_beep", &nvs_tmp);
    if (nvs_tmp != hr_beep) nvs_set_u8(sys_nvs_handle, "hr_beep", hr_beep);

    uint8_t paper_speed = (uint8_t)get_ecg_paper_speed();
    nvs_get_u8(sys_nvs_handle, "paper_speed", &nvs_tmp);
    if (nvs_tmp != paper_speed) nvs_set_u8(sys_nvs_handle, "paper_speed", paper_speed);

    uint8_t wifi_mode = (uint8_t)get_wifi_mode();
    nvs_get_u8(sys_nvs_handle, "wifi_mode", &nvs_tmp);
    if (nvs_tmp != wifi_mode) nvs_set_u8(sys_nvs_handle, "wifi_mode", wifi_mode);

    uint8_t fs_auto = (uint8_t)settings2_page_get_autostart_setting();
    nvs_get_u8(sys_nvs_handle, "fs_auto", &nvs_tmp);
    if (nvs_tmp != fs_auto) nvs_set_u8(sys_nvs_handle, "fs_auto", fs_auto);

    data_logger_t data_logger_status;
    data_logger_get_status_copy(&data_logger_status);
    uint32_t rec_duration = data_logger_status.user_defined_duration_min;
    nvs_get_u32(sys_nvs_handle, "rec_duration", &nvs_tmp32);
    if (nvs_tmp32 != rec_duration) {
        nvs_set_u32(sys_nvs_handle, "rec_duration", rec_duration);
        ESP_LOGI(TAG, "Recording duration set to %" PRIu32 " minutes.", rec_duration);
    }
    uint8_t rec_countdown = data_logger_status.prepare_countdown_sec;
    nvs_get_u8(sys_nvs_handle, "rec_cntdn", &nvs_tmp);
    if (nvs_tmp != rec_countdown) {
        nvs_set_u8(sys_nvs_handle, "rec_cntdn", rec_countdown);
        ESP_LOGI(TAG, "Recording countdown set to %u seconds.", rec_countdown);
    }
    uint32_t rec_split_size = data_logger_status.user_defined_split_size_mb;
    nvs_get_u32(sys_nvs_handle, "rec_split", &nvs_tmp32);
    if (nvs_tmp32 != rec_split_size) {
        nvs_set_u32(sys_nvs_handle, "rec_split", rec_split_size);
        ESP_LOGI(TAG, "File split size set to %" PRIu32 " MB.", rec_split_size);
    }
    uint8_t rec_format = (uint8_t)data_logger_status.selected_format;
    nvs_get_u8(sys_nvs_handle, "rec_formt", &nvs_tmp);
    if (nvs_tmp != rec_format) {
        nvs_set_u8(sys_nvs_handle, "rec_formt", rec_format);
        if (data_logger_status.selected_format == DATA_FORMAT_CSV) {
            ESP_LOGI(TAG, "Data format set to CSV.");
        } else {
            ESP_LOGI(TAG, "Data format set to BDF.");
        }
    }

    nvs_commit(sys_nvs_handle);
}

void save_timezone_to_nvs(const char *timezone) {
    nvs_set_str(sys_nvs_handle, "time_zone", timezone);
    nvs_commit(sys_nvs_handle);
}

esp_err_t load_timezone_from_nvs(char *buffer, size_t max_len) {
    size_t required_size = 0;
    esp_err_t err = nvs_get_str(sys_nvs_handle, "time_zone", NULL, &required_size); // 获取所需大小
    if (err != ESP_OK || required_size > max_len) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE("nvs", "Failed to get timezone size from NVS: %s", esp_err_to_name(err));
        }
        return err == ESP_OK ? ESP_FAIL : err; // 大小超限，返回失败
    }
    err = nvs_get_str(sys_nvs_handle, "time_zone", buffer, &required_size); // 获取字符串
    if (err == ESP_OK) {
        ESP_LOGI("nvs", "Loaded timezone '%s' from NVS.", buffer);
    }
    return err;
}
