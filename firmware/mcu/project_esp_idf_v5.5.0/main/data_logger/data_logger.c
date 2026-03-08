#include "data_logger.h"
#include "app_file_server.h"
#include "beep.h"
#include "data_formatter.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "mcu_fpga_comm.h"
#include "nvs_app.h"
#include "sdmmc_cmd.h"
#include "sntp_app.h"
#include "wifi_app.h"
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static const char *TAG = "data_logger";
data_logger_t g_data_logger;
static data_formatter_t *g_current_formatter;
static TimerHandle_t g_recording_timer_handle = NULL;

/*****************************************************************************************************************/
static esp_err_t init_fat(void) {
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    esp_err_t ret = ESP_FAIL;
    sdmmc_card_t *card;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16 * 1024};
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = 40 * 1000;
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = SDCARD_SDIO_CLK_PIN;
    slot_config.cmd = SDCARD_SDIO_CMD_PIN;
    slot_config.d0 = SDCARD_SDIO_D0_PIN;
    slot_config.gpio_cd = SDCARD_DET_PIN;
    slot_config.width = 1;
    snprintf(g_data_logger.disk_path, sizeof(g_data_logger.disk_path), "/sdcard");
    ret = esp_vfs_fat_sdmmc_mount(g_data_logger.disk_path, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGW(TAG, "Failed to mount filesystem.");
        } else {
            ESP_LOGW(TAG, "Failed to initialize the card (%s).", esp_err_to_name(ret));
        }
        return ret;
    }
    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}

static esp_err_t update_sdcard_capacity(void) {
    FATFS *fs;
    DWORD free_clusters;
    FRESULT res = f_getfree(g_data_logger.disk_path, &free_clusters, &fs);
    if (res != FR_OK) {
        ESP_LOGW(TAG, "f_getfree failed, error: %d", res);
        return ESP_FAIL;
    }
    uint64_t total_sectors = (uint64_t)(fs->n_fatent - 2) * fs->csize;
    uint64_t free_sectors = (uint64_t)free_clusters * fs->csize;
    uint64_t sector_size = fs->ssize;
    g_data_logger.sdcard_total_space_bytes = total_sectors * sector_size;
    g_data_logger.sdcard_free_space_bytes = free_sectors * sector_size;
    g_data_logger.sdcard_used_space_bytes = (total_sectors - free_sectors) * sector_size;
    if (g_data_logger.sdcard_free_space_bytes < 1024 * 1024) { // less than 1MB
        g_data_logger.last_error = LOGGER_ERROR_SD_FULL;
    } else g_data_logger.last_error = LOGGER_ERROR_NONE;
    return ESP_OK;
}

/*****************************************************************************************************************/
static TaskHandle_t data_logger_task_handle;
static SemaphoreHandle_t g_logger_mutex;
typedef enum { LOGGER_CMD_START, LOGGER_CMD_STOP, LOGGER_CMD_PAUSE, LOGGER_CMD_RESUME } logger_command_t;

static bool create_new_filename(const char *prefix, const char *extension) {
    int32_t max_index_found = -1;
    DIR *dir = opendir(g_data_logger.disk_path); // 遍历目录，找到当前存在的最大文件序号
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory: %s", g_data_logger.disk_path);
        return false;
    }
    struct dirent *entry;
    size_t prefix_len = strlen(prefix);
    while ((entry = readdir(dir)) != NULL) {
        // 检查文件名是否以前缀开头，并且后面紧跟着是数字
        if (strncmp(entry->d_name, prefix, prefix_len) == 0 && isdigit((unsigned char)entry->d_name[prefix_len])) {
            int current_index = atoi(&entry->d_name[prefix_len]); // 从文件名 "ECGXXX_..." 中提取序号 "XXX"
            if (current_index > max_index_found) max_index_found = current_index;
        }
    }
    closedir(dir);
    uint32_t next_index = (max_index_found == -1) ? 0 : max_index_found + 1; // 根据找到的最大序号得到下一个可用的序号
    if (next_index >= DATA_LOGGER_MAX_FILES) {                               // 检查序号是否超出上限
        ESP_LOGE(TAG, "Next index %" PRIu32 " has reached or exceeded the limit of %d.", next_index, DATA_LOGGER_MAX_FILES);
        return false;
    }
    struct tm timeinfo; // 获取时间并拼接最终文件名
    sntp_get_timeinfo(&timeinfo);
    char time_str_buf[20];
    strftime(time_str_buf, sizeof(time_str_buf), "%Y-%m-%d_%H-%M-%S", &timeinfo);
    snprintf(g_data_logger.current_file_name, sizeof(g_data_logger.current_file_name), "%s/%s%03" PRIu32 "_%s%s",
             g_data_logger.disk_path, prefix, next_index, time_str_buf, extension);
    ESP_LOGI(TAG, "New filename created: %s", g_data_logger.current_file_name);
    return true;
}

static bool open_recording_file(void) {
    int fd = open(g_data_logger.current_file_name, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open file: %s", g_data_logger.current_file_name);
        return false;
    }
    g_data_logger.current_file_descriptor = fd;
    g_data_logger.total_bytes_written = 0;
    g_data_logger.total_samples_written = 0;
    return true;
}

static void close_recording_file(void) {
    if (g_data_logger.current_file_descriptor >= 0) {
        close(g_data_logger.current_file_descriptor);
        g_data_logger.current_file_descriptor = -1; // 标记为无效
        ESP_LOGI(TAG, "File closed: %s", g_data_logger.current_file_name);
    }
}

/*****************************************************************************************************************/
static void handle_start_command(void) {
    if (g_data_logger.is_recording) return;
    if ((!acquire_file_server_lock()) && (wifi_status.is_start)) {
        g_data_logger.last_error = LOGGER_ERROR_SD_BUSY;
        ESP_LOGW(TAG, "Cannot start recording: SD card is busy by file server.");
        return;
    }
    if (g_data_logger.selected_format == DATA_FORMAT_CSV) {
        g_current_formatter = (data_formatter_t *)&csv_formatter;
    } else {
        g_current_formatter = (data_formatter_t *)&bdf_formatter;
    }
    bool success = false;
    do {
        if (update_sdcard_capacity() != ESP_OK) {
            g_data_logger.last_error = LOGGER_ERROR_SD_INIT_FAIL;
            ESP_LOGW(TAG, "Cannot start recording: Failed to update SD card capacity.");
            break;
        }
        if (g_data_logger.last_error == LOGGER_ERROR_SD_FULL) {
            ESP_LOGW(TAG, "Cannot start recording: SD card is full.");
            break;
        }
        if (!create_new_filename("ECG", g_current_formatter->file_extension) || !open_recording_file()) {
            g_data_logger.last_error = LOGGER_ERROR_FILE_OPEN;
            break;
        }
        if (g_current_formatter->write_header) {
            int32_t header_bytes = g_current_formatter->write_header(g_data_logger.current_file_descriptor, &g_data_logger);
            if (header_bytes < 0) {
                g_data_logger.last_error = LOGGER_ERROR_FILE_WRITE;
                ESP_LOGE(TAG, "Failed to write header.");
                close_recording_file();
                break;
            }
            g_data_logger.total_bytes_written += header_bytes;
        }
        xStreamBufferReset(raw_ecg_stream_buffer);
        success = true;
    } while (0);
    if (success) {
        ESP_LOGI(TAG, "Recording started.");
        atomic_store(&g_data_logger.is_recording, true);
        atomic_store(&g_data_logger.is_paused, false);
        g_data_logger.last_error = LOGGER_ERROR_NONE;
        g_data_logger.elapsed_recording_time_sec = 0;
    } else {
        ESP_LOGE(TAG, "Failed to start recording.");
        release_file_server_lock();
    }
}
static void handle_stop_command(void) {
    if (!atomic_load(&g_data_logger.is_recording)) return;
    ESP_LOGI(TAG, "STOP command received. Finalizing recording...");
    atomic_store(&g_data_logger.is_recording, false);
    atomic_store(&g_data_logger.is_paused, false);
}
static void handle_pause_command(void) {
    if (atomic_load(&g_data_logger.is_recording) && !atomic_load(&g_data_logger.is_paused)) {
        ESP_LOGI(TAG, "Recording paused.");
        atomic_store(&g_data_logger.is_paused, true);
    }
}
static void handle_resume_command(void) {
    if (atomic_load(&g_data_logger.is_recording) && atomic_load(&g_data_logger.is_paused)) {
        ESP_LOGI(TAG, "Recording resumed.");
        atomic_store(&g_data_logger.is_paused, false);
    }
}
static void run_recording_loop(void) {
    flush_request_t request;
    ESP_LOGI(TAG, "Entering recording loop.");
    while (1) {
        uint32_t received_command;
        // 非阻塞地检查是否有PAUSE或STOP命令
        if (xTaskNotifyWait(0, 0, &received_command, 0) == pdTRUE) {
            // 将命令重新通知给自己，让外循环可以接收处理
            xTaskNotify(data_logger_task_handle, received_command, eSetValueWithOverwrite);
            break; // 退出内循环
        }
        if (xQueueReceive(g_flush_queue, &request, pdMS_TO_TICKS(100)) == pdTRUE) {
            uint32_t sample_count = request.data_size / sizeof(int32_t);
            int32_t bytes_written = g_current_formatter->write_data(g_data_logger.current_file_descriptor,
                                                                    (const int32_t *)request.buffer_ptr, sample_count);
            if (bytes_written > 0) {
                g_data_logger.total_bytes_written += bytes_written;
                g_data_logger.total_samples_written += sample_count;
                // 每次数据成功写入后，调用更新函数，确保文件头始终反映已写入的最新状态
                if (g_current_formatter->update_metadata) {
                    g_current_formatter->update_metadata(g_data_logger.current_file_descriptor, &g_data_logger);
                }
            } else {
                g_data_logger.last_error = LOGGER_ERROR_FILE_WRITE;
                ESP_LOGE(TAG, "Failed to write buffer to SD card. Stopping recording.");
                stop_data_logger();
                break;
            }
            uint32_t split_size_mb = g_data_logger.user_defined_split_size_mb;
            if (split_size_mb > 0) { // 若启用了基于大小的分割
                uint64_t split_size_bytes = (uint64_t)split_size_mb * 1024 * 1024;
                if (g_data_logger.total_bytes_written > split_size_bytes) { // 若文件大小超过分割阈值
                    ESP_LOGI(TAG, "File size limit reached (%" PRIu64 " bytes). Creating new segment file...",
                             g_data_logger.total_bytes_written);
                    close_recording_file();
                    if (!create_new_filename("ECG", g_current_formatter->file_extension) || !open_recording_file()) {
                        g_data_logger.last_error = LOGGER_ERROR_FILE_OPEN;
                        ESP_LOGE(TAG, "Failed to create new segment file. Stopping recording.");
                        stop_data_logger();
                        break;
                    }
                    int32_t header_bytes =
                        g_current_formatter->write_header(g_data_logger.current_file_descriptor, &g_data_logger);
                    if (header_bytes < 0) {
                        g_data_logger.last_error = LOGGER_ERROR_FILE_WRITE;
                        ESP_LOGE(TAG, "Failed to write header for new segment. Stopping recording.");
                        stop_data_logger();
                        break;
                    }
                    g_data_logger.total_bytes_written += header_bytes;
                }
            }
        }
    }
    ESP_LOGI(TAG, "Exiting recording loop.");
}

static void data_logger_task(void *arg) {
    while (1) {
        uint32_t received_command;
        xTaskNotifyWait(0, UINT32_MAX, &received_command, portMAX_DELAY);
        if (xSemaphoreTake(g_logger_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            bool is_recording_before = atomic_load(&g_data_logger.is_recording); // 记录处理命令前的状态
            switch (received_command) {
            case LOGGER_CMD_START: handle_start_command(); break;
            case LOGGER_CMD_STOP: handle_stop_command(); break;
            case LOGGER_CMD_PAUSE: handle_pause_command(); break;
            case LOGGER_CMD_RESUME: handle_resume_command(); break;
            default: break;
            }
            bool is_recording_after = atomic_load(&g_data_logger.is_recording);
            bool is_paused_after = atomic_load(&g_data_logger.is_paused);
            bool should_run_loop = is_recording_after && !is_paused_after;
            xSemaphoreGive(g_logger_mutex);

            if (should_run_loop) run_recording_loop();

            if (is_recording_before && !is_recording_after) { // 处理停止的善后
                vTaskDelay(300);                              // 给adc_collection_task一点时间检测状态变化并发送最后的数据
                ESP_LOGI(TAG, "Finalizing and closing file...");
                flush_request_t request;
                while (xQueueReceive(g_flush_queue, &request, 0) == pdTRUE) {
                    uint32_t sample_count = request.data_size / sizeof(int32_t);
                    int32_t bytes_written = g_current_formatter->write_data(g_data_logger.current_file_descriptor,
                                                                            (const int32_t *)request.buffer_ptr, sample_count);
                    if (bytes_written > 0) {
                        g_data_logger.total_bytes_written += bytes_written;
                        g_data_logger.total_samples_written += sample_count;
                    }
                }
                if (g_current_formatter->finalize_file) {
                    if (g_current_formatter->finalize_file(g_data_logger.current_file_descriptor, &g_data_logger) < 0) {
                        g_data_logger.last_error = LOGGER_ERROR_FILE_WRITE;
                        ESP_LOGE(TAG, "Formatter finalize function failed.");
                    }
                }
                close_recording_file();
                release_file_server_lock();
            }
        }
    }
}

static void vRecordingTimerCallback(TimerHandle_t xTimer) {
    if (atomic_load(&g_data_logger.is_recording) && !atomic_load(&g_data_logger.is_paused)) {
        uint32_t elapsed_sec = atomic_fetch_add(&g_data_logger.elapsed_recording_time_sec, 1) + 1;
        uint32_t target_duration_min = g_data_logger.user_defined_duration_min;
        if (target_duration_min > 0) { // 检查是否设置了有效的自动停止时长
            uint32_t target_duration_sec = target_duration_min * 60;
            if (elapsed_sec >= target_duration_sec) { // 如果已录制时长达到或超过目标时长，则发送停止命令
                ESP_LOGI(TAG, "Target duration of %" PRIu32 " minutes reached. Auto-stopping recording.", target_duration_min);
                stop_data_logger();
                back_home_long_beep();
            }
        }
    }
}

/*****************************************************************************************************************/
void init_data_logger(void) {
    g_logger_mutex = xSemaphoreCreateMutex();
    memset(&g_data_logger, 0, sizeof(data_logger_t));
    if (init_fat() == ESP_OK) {
        g_data_logger.is_sdcard_present = true;
        update_sdcard_capacity();
    } else {
        g_data_logger.is_sdcard_present = false;
        g_data_logger.last_error = LOGGER_ERROR_SD_INIT_FAIL;
    }
    g_data_logger.selected_format = DATA_FORMAT_CSV; // 默认使用CSV格式
    g_data_logger.user_defined_split_size_mb = 0;    // 默认不分割
    g_data_logger.user_defined_duration_min = 0;     // 默认不限制记录时长
    g_data_logger.prepare_countdown_sec = 3;         // 默认倒计时3秒
    init_adc_collection_task();
    xTaskCreate(data_logger_task, "data_logger", 4096, NULL, 5, &data_logger_task_handle);
    g_recording_timer_handle = xTimerCreate("rec_timer", 1000, pdTRUE, (void *)0, vRecordingTimerCallback);
    if (g_recording_timer_handle != NULL) xTimerStart(g_recording_timer_handle, 0);
}

static void data_logger_send_command(logger_command_t command) {
    if (data_logger_task_handle != NULL) {
        xTaskNotify(data_logger_task_handle, command, eSetValueWithOverwrite);
    }
}
void start_data_logger(void) { data_logger_send_command(LOGGER_CMD_START); }
void stop_data_logger(void) { data_logger_send_command(LOGGER_CMD_STOP); }
void pause_data_logger(void) { data_logger_send_command(LOGGER_CMD_PAUSE); }
void resume_data_logger(void) { data_logger_send_command(LOGGER_CMD_RESUME); }

void data_logger_get_status_copy(data_logger_t *status_copy) {
    if (g_logger_mutex == NULL || status_copy == NULL) return;
    xSemaphoreTake(g_logger_mutex, portMAX_DELAY);
    memcpy(status_copy, &g_data_logger, sizeof(data_logger_t));
    status_copy->is_recording = atomic_load(&g_data_logger.is_recording);
    status_copy->is_paused = atomic_load(&g_data_logger.is_paused);
    status_copy->elapsed_recording_time_sec = atomic_load(&g_data_logger.elapsed_recording_time_sec);
    xSemaphoreGive(g_logger_mutex);
}

void clear_data_logger_error(void) {
    if (g_logger_mutex == NULL) return;
    if (xSemaphoreTake(g_logger_mutex, portMAX_DELAY) == pdTRUE) {
        if (g_data_logger.last_error != LOGGER_ERROR_NONE) {
            g_data_logger.last_error = LOGGER_ERROR_NONE;
        }
        xSemaphoreGive(g_logger_mutex);
    }
}

void data_logger_set_duration(uint32_t minutes) {
    if (g_logger_mutex == NULL) return;
    if (xSemaphoreTake(g_logger_mutex, portMAX_DELAY) == pdTRUE) {
        g_data_logger.user_defined_duration_min = minutes;
        xSemaphoreGive(g_logger_mutex);
    }
}

void data_logger_set_format(data_format_t format) {
    if (g_logger_mutex == NULL) return;
    if (xSemaphoreTake(g_logger_mutex, portMAX_DELAY) == pdTRUE) {
        if (format == DATA_FORMAT_CSV) {
            g_data_logger.selected_format = DATA_FORMAT_CSV;
        } else {
            g_data_logger.selected_format = DATA_FORMAT_BDF;
        }
        xSemaphoreGive(g_logger_mutex);
    }
}

void data_logger_set_countdown(uint8_t seconds) {
    if (g_logger_mutex == NULL) return;
    if (xSemaphoreTake(g_logger_mutex, portMAX_DELAY) == pdTRUE) {
        g_data_logger.prepare_countdown_sec = seconds;
        xSemaphoreGive(g_logger_mutex);
    }
}

void data_logger_set_split_size(uint32_t size_mb) {
    if (g_logger_mutex == NULL) return;
    if (xSemaphoreTake(g_logger_mutex, portMAX_DELAY) == pdTRUE) {
        g_data_logger.user_defined_split_size_mb = size_mb;
        xSemaphoreGive(g_logger_mutex);
    }
}
