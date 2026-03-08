#include "data_logger.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "mcu_fpga_comm.h"
#include <stdatomic.h>
#include <string.h>

static const char *TAG = "adc_collection";
EXT_RAM_BSS_ATTR static uint8_t ecg_psram_buffer[PSRAM_BUFFER_TOTAL_SIZE];
QueueHandle_t g_flush_queue;
TaskHandle_t g_adc_collection_task_handle;

static void adc_collection_task(void *pvParameters) {
    uint8_t *ping_buffer = ecg_psram_buffer;
    uint8_t *pong_buffer = ecg_psram_buffer + PSRAM_BUFFER_HALF_SIZE;
    uint8_t *current_write_buffer = ping_buffer;
    size_t current_buffer_offset = 0;
    static bool was_recording = false; // 上一次循环时的录制状态
    ESP_LOGI(TAG, "ADC Collection Task started.");
    while (1) {
        bool is_recording_now = atomic_load(&g_data_logger.is_recording);
        bool is_paused_now = atomic_load(&g_data_logger.is_paused);
        if (is_recording_now && !is_paused_now) { // 只有在录制且未暂停时，才去尝试接收和处理数据
            was_recording = true;
            int32_t received_sample;
            size_t bytes_received = // 使用100ms的延时来等待数据，使任务可以周期性地检查状态变化
                xStreamBufferReceive(raw_ecg_stream_buffer, &received_sample, sizeof(received_sample), 100);
            if (bytes_received > 0) {
                if (current_buffer_offset + sizeof(int32_t) > PSRAM_BUFFER_HALF_SIZE) { // 如果半区写满
                    ESP_LOGI(TAG, "Buffer half at %p is full. Notifying data_logger to flush.", current_write_buffer);
                    flush_request_t request = {.buffer_ptr = current_write_buffer, .data_size = current_buffer_offset};
                    xQueueSend(g_flush_queue, &request, portMAX_DELAY);
                    current_write_buffer = (current_write_buffer == ping_buffer) ? pong_buffer : ping_buffer;
                    current_buffer_offset = 0;
                }
                memcpy(current_write_buffer + current_buffer_offset, &received_sample, sizeof(int32_t));
                current_buffer_offset += sizeof(int32_t);
            }
        } else {
            if (was_recording && !is_recording_now) { // 如果上一轮是录制中，而当前已停止，说明stop命令被触发了
                if (current_buffer_offset > 0) {      // 如果当前缓冲区有剩余数据，则执行最后一次flush
                    flush_request_t final_request = {.buffer_ptr = current_write_buffer, .data_size = current_buffer_offset};
                    xQueueSend(g_flush_queue, &final_request, portMAX_DELAY);
                    current_write_buffer = ping_buffer;
                    current_buffer_offset = 0;
                }
            }
            was_recording = is_recording_now; // 更新上一轮的状态
            vTaskDelay(100);
        }
    }
}

void init_adc_collection_task(void) {
    g_flush_queue = xQueueCreate(2, sizeof(flush_request_t));
    xTaskCreate(adc_collection_task, "adc_collection", 4096, NULL, 10, &g_adc_collection_task_handle);
}
