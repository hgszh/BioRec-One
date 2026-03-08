#ifndef DATA_LOGGER_H_
#define DATA_LOGGER_H_

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDCARD_SDIO_CLK_PIN 14
#define SDCARD_SDIO_CMD_PIN 21
#define SDCARD_SDIO_D0_PIN 13
#define SDCARD_DET_PIN 12

#define DATA_LOGGER_MAX_FILES 3000

typedef enum {
    LOGGER_ERROR_NONE,         // 正常
    LOGGER_ERROR_SD_INIT_FAIL, // SD卡初始化失败
    LOGGER_ERROR_SD_FULL,      // SD卡容量已满
    LOGGER_ERROR_SD_BUSY,      // SD卡正忙
    LOGGER_ERROR_FILE_OPEN,    // 文件打开失败
    LOGGER_ERROR_FILE_WRITE    // 文件写入失败
} data_logger_error_t;

typedef enum {
    DATA_FORMAT_CSV, // 默认
    DATA_FORMAT_BDF
} data_format_t;

typedef struct {
    data_logger_error_t last_error; // 最近一次发生的错误类型
    atomic_bool is_recording;       // 是否处于一个录制会话中
    atomic_bool is_paused;          // 当前录制会话是否已暂停

    bool is_sdcard_present;
    char disk_path[12];

    uint64_t sdcard_total_space_bytes;
    uint64_t sdcard_free_space_bytes;
    uint64_t sdcard_used_space_bytes;

    int current_file_descriptor;            // open() 返回的文件描述符, -1无效
    char current_file_name[64];             // 当前文件的完整路径
    uint64_t total_bytes_written;           // 累计当前文件写入的字节数
    uint64_t total_samples_written;         // 累计当前文件写入的样本数
    atomic_uint elapsed_recording_time_sec; // 从开始记录到现在的总时长（秒）

    data_format_t selected_format;       // 用户选择的文件格式
    uint8_t prepare_countdown_sec;       // 预备倒计时时长
    uint32_t user_defined_split_size_mb; // 文件分割大小（MB），0表示不分割
    uint32_t user_defined_duration_min;  // 用户设定的本次记录时长（分钟），0表示无限长
} data_logger_t;
extern data_logger_t g_data_logger;

/********************************************************************************************/
void init_data_logger(void);
void start_data_logger(void);
void stop_data_logger(void);
void pause_data_logger(void);
void resume_data_logger(void);
void data_logger_get_status_copy(data_logger_t *status_copy);
void clear_data_logger_error(void);
void data_logger_set_duration(uint32_t minutes);
void data_logger_set_format(data_format_t format);
void data_logger_set_countdown(uint8_t seconds);
void data_logger_set_split_size(uint32_t size_mb);

/********************************************************************************************/
extern TaskHandle_t g_adc_collection_task_handle;
typedef struct {
    uint8_t *buffer_ptr;
    size_t data_size;
} flush_request_t;
extern QueueHandle_t g_flush_queue;
#define PSRAM_BUFFER_HALF_SIZE (512 * 1024)
#define PSRAM_BUFFER_TOTAL_SIZE (PSRAM_BUFFER_HALF_SIZE * 2)
void init_adc_collection_task(void);

#ifdef __cplusplus
}
#endif

#endif
