/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Console example — various system commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "argtable3/argtable3.h"
#include "cmd_system.h"
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "sntp_app.h"

#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
#define WITH_TASKS_INFO 1
#endif

static const char *TAG = "cmd_system_common";

static void register_free(void);
static void register_heap(void);
static void register_version(void);
static void register_restart(void);
static void register_tasks(void);
static void register_log_level(void);
static void register_tzset(void);
static void register_timenow(void);
static void register_temperature(void);
static void register_beep(void);
static void register_stats(void);

void register_system_common(void) {
    register_tasks();
    register_stats();
    register_free();
    register_heap();
    register_temperature();
    register_tzset();
    register_timenow();
    register_restart();
    register_log_level();
    register_beep();
    register_version();
}

/* 'version' command */
static int get_version(int argc, char **argv) {
    const char *model;
    esp_chip_info_t info;
    uint32_t flash_size;
    esp_chip_info(&info);

    switch (info.model) {
    case CHIP_ESP32: model = "ESP32"; break;
    case CHIP_ESP32S2: model = "ESP32-S2"; break;
    case CHIP_ESP32S3: model = "ESP32-S3"; break;
    case CHIP_ESP32C3: model = "ESP32-C3"; break;
    case CHIP_ESP32H2: model = "ESP32-H2"; break;
    case CHIP_ESP32C2: model = "ESP32-C2"; break;
    case CHIP_ESP32P4: model = "ESP32-P4"; break;
    case CHIP_ESP32C5: model = "ESP32-C5"; break;
    default: model = "Unknown"; break;
    }

    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return 1;
    }
    printf("IDF Version:%s\r\n", esp_get_idf_version());
    printf("Chip info:\r\n");
    printf("\tmodel:%s\r\n", model);
    printf("\tcores:%d\r\n", info.cores);
    printf("\tfeature:%s%s%s%s%" PRIu32 "%s\r\n", info.features & CHIP_FEATURE_WIFI_BGN ? "/802.11bgn" : "",
           info.features & CHIP_FEATURE_BLE ? "/BLE" : "", info.features & CHIP_FEATURE_BT ? "/BT" : "",
           info.features & CHIP_FEATURE_EMB_FLASH ? "/Embedded-Flash:" : "/External-Flash:", flash_size / (1024 * 1024), " MB");
    printf("\trevision number:%d\r\n", info.revision);
    return 0;
}

static void register_version(void) {
    const esp_console_cmd_t cmd = {
        .command = "version",
        .help = "Get version of chip and SDK",
        .hint = NULL,
        .func = &get_version,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** 'restart' command restarts the program */
static int restart(int argc, char **argv) {
    ESP_LOGI(TAG, "Restarting");
    esp_restart();
}

static void register_restart(void) {
    const esp_console_cmd_t cmd = {
        .command = "restart",
        .help = "Software reset of the chip",
        .hint = NULL,
        .func = &restart,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** 'free' command prints available heap memory */
static int free_mem(int argc, char **argv) {
    printf("%" PRIu32 "\n", esp_get_free_heap_size());
    return 0;
}

static void register_free(void) {
    const esp_console_cmd_t cmd = {
        .command = "free",
        .help = "Get the current size of free heap memory",
        .hint = NULL,
        .func = &free_mem,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/* 'heap' command prints minimum heap size */
static int heap_size(int argc, char **argv) {
    uint32_t heap_size = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    printf("min heap size: %" PRIu32 "\n", heap_size);
    return 0;
}

static void register_heap(void) {
    const esp_console_cmd_t heap_cmd = {
        .command = "heap",
        .help = "Get minimum size of free heap memory that was available during program execution",
        .hint = NULL,
        .func = &heap_size,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&heap_cmd));
}

/* 'tasks' command prints information about running tasks */
static int tasks_info(int argc, char **argv) {
    const size_t bytes_per_task = 40; /* see vTaskList description */
    char *task_list_buffer = malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == NULL) {
        ESP_LOGE(TAG, "failed to allocate buffer for vTaskList output");
        return 1;
    }
    fputs("Task Name\tStatus\tPrio\tHWM\tTask#", stdout);
    fputs("\tAffinity", stdout);
    fputs("\n", stdout);
    vTaskList(task_list_buffer);
    fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    return 0;
}

static void register_tasks(void) {
    const esp_console_cmd_t cmd = {
        .command = "tasks",
        .help = "Get information about running tasks",
        .hint = NULL,
        .func = &tasks_info,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/* 'stats' command prints runtime statistics about all tasks */
#define STATS_BUFFER_SIZE 1024
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static int runtime_stats_info(int argc, char **argv) {
    UBaseType_t num_of_tasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *task_status_array = pvPortMalloc(num_of_tasks * sizeof(TaskStatus_t));
    if (task_status_array == NULL) {
        printf("Error: Failed to allocate memory for task status array\n");
        return 1;
    }
    uint32_t total_run_time;
    num_of_tasks = uxTaskGetSystemState(task_status_array, num_of_tasks, &total_run_time);
    printf("=========================================================\n");
    printf("%-20s %12s %8s\n", "Task Name", "Abs Time", "%Time");
    printf("---------------------------------------------------------\n");
    if (total_run_time > 0) {
        for (int i = 0; i < num_of_tasks; i++) {
            TaskStatus_t *status = &task_status_array[i];
            uint32_t percent = (uint64_t)status->ulRunTimeCounter * 100 / total_run_time;
            printf("%-20s %12lu", status->pcTaskName, status->ulRunTimeCounter);
            if (percent > 0) {
                printf(" %7lu%%\n", percent);
            } else {
                if (status->ulRunTimeCounter > 0) printf(" %8s\n", "<1%");
                else printf(" %8s\n", "0%");
            }
        }
    }
    printf("---------------------------------------------------------\n");
    printf("* Abs Time: Absolute run time (in timer ticks)\n");
    printf("=========================================================\n");
    vPortFree(task_status_array);
    return 0;
}
static void register_stats(void) {
    const esp_console_cmd_t cmd = {
        .command = "stats",
        .help = "Get run time statistics about running tasks",
        .hint = NULL,
        .func = &runtime_stats_info,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** log_level command changes log level via esp_log_level_set */
static struct {
    struct arg_str *tag;
    struct arg_str *level;
    struct arg_end *end;
} log_level_args;

static const char *s_log_level_names[] = {"none", "error", "warn", "info", "debug", "verbose"};

static int log_level(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void **)&log_level_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, log_level_args.end, argv[0]);
        return 1;
    }
    assert(log_level_args.tag->count == 1);
    assert(log_level_args.level->count == 1);
    const char *tag = log_level_args.tag->sval[0];
    const char *level_str = log_level_args.level->sval[0];
    esp_log_level_t level;
    size_t level_len = strlen(level_str);
    for (level = ESP_LOG_NONE; level <= ESP_LOG_VERBOSE; level++) {
        if (memcmp(level_str, s_log_level_names[level], level_len) == 0) {
            break;
        }
    }
    if (level > ESP_LOG_VERBOSE) {
        printf("Invalid log level '%s', choose from none|error|warn|info|debug|verbose\n", level_str);
        return 1;
    }
    if (level > CONFIG_LOG_MAXIMUM_LEVEL) {
        printf("Can't set log level to %s, max level limited in menuconfig to %s. "
               "Please increase CONFIG_LOG_MAXIMUM_LEVEL in menuconfig.\n",
               s_log_level_names[level], s_log_level_names[CONFIG_LOG_MAXIMUM_LEVEL]);
        return 1;
    }
    esp_log_level_set(tag, level);
    return 0;
}

static void register_log_level(void) {
    log_level_args.tag = arg_str1(NULL, NULL, "<tag|*>", "Log tag to set the level for, or * to set for all tags");
    log_level_args.level =
        arg_str1(NULL, NULL, "<none|error|warn|debug|verbose>", "Log level to set. Abbreviated words are accepted.");
    log_level_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {.command = "log_level",
                                   .help = "Set log level for all tags or a specific tag.",
                                   .hint = NULL,
                                   .func = &log_level,
                                   .argtable = &log_level_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** timenow */
static int timenow(int argc, char **argv) {
    sntp_print_time_now();
    return 0;
}

static void register_timenow(void) {
    const esp_console_cmd_t cmd = {
        .command = "timenow",
        .help = "Get current time",
        .hint = NULL,
        .func = &timenow,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** temperature */
#include "driver/temperature_sensor.h"
static temperature_sensor_handle_t temp_sensor = NULL;
static int get_chip_temperature(int argc, char **argv) {
    float tsens_value;
    temperature_sensor_get_celsius(temp_sensor, &tsens_value);
    printf("Chip temperature: %.2f °C\n", tsens_value);
    return 0;
}

static void register_temperature(void) {
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 80);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
    const esp_console_cmd_t cmd = {
        .command = "temperature",
        .help = "Get chip temperature",
        .hint = NULL,
        .func = &get_chip_temperature,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/** beep */
#include "ui_hal.h"
static struct {
    struct arg_int *freq;
    struct arg_int *duty;
    struct arg_int *duration;
    struct arg_end *end;
} beep_args;
static int beep(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void **)&beep_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, beep_args.end, argv[0]);
        return 1;
    }
    int freq = beep_args.freq->ival[0];
    int duty = beep_args.duty->ival[0];
    int duration_ms = beep_args.duration->ival[0];
    if ((freq > 0x3FFF) || (duty > 0xFF) || (duration_ms > 0x3FF)) {
        printf("Error: Parameter out of range. Valid ranges: freq <= 16383, duty <= 255, duration <= 1023 ms\n");
        return 1;
    }
    send_buzzer_cmd(freq, duty, duration_ms);
    return 0;
}
static void register_beep(void) {
    beep_args.freq = arg_int1(NULL, NULL, "<freq_hz>", "Beep frequency in Hz (0–16383)");
    beep_args.duty = arg_int1(NULL, NULL, "<duty>", "Duty cycle in percent (0–255)");
    beep_args.duration = arg_int1(NULL, NULL, "<duration_ms>", "Duration in milliseconds (0–1023)");
    beep_args.end = arg_end(3);
    const esp_console_cmd_t cmd = {
        .command = "beep",
        .help = "Generate a beep sound from the buzzer.",
        .hint = NULL,
        .func = &beep,
        .argtable = &beep_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/* timezone */
#include "nvs_app.h"
static struct {
    struct arg_str *posix_string;
    struct arg_end *end;
} tzset_args;
static int tzset_cmd_handler(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void **)&tzset_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, tzset_args.end, argv[0]);
        return 1;
    }
    const char *new_posix_str = tzset_args.posix_string->sval[0];
    size_t len = strlen(new_posix_str);
    if (len > TZ_STRING_MAX_LEN) {
        printf("Error: string is too long (max %d chars).\n", TZ_STRING_MAX_LEN);
        return 1;
    }
    save_timezone_to_nvs(new_posix_str);
    apply_timezone(new_posix_str);
    printf("TZ environment variable set to '%s' and saved.\n", new_posix_str);
    return 0;
}

static void register_tzset(void) {
    tzset_args.posix_string = arg_str1(NULL, NULL, "<posix_string>",
                                       "E.g. 'CST-8' (China), 'JST-9' (Japan), "
                                       "'AEST-10AEDT,M10.1.0,M4.1.0/3' (Sydney), 'EST5EDT,M3.2.0,M11.1.0' (US East).");
    tzset_args.end = arg_end(1);
    const esp_console_cmd_t cmd = {
        .command = "tzset",
        .help = "Set the TZ environment variable.",
        .hint = NULL,
        .func = &tzset_cmd_handler,
        .argtable = &tzset_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
