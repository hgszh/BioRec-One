#include "data_formatter.h"
#include "esp_log.h"
#include "mcu_fpga_comm.h"
#include "sntp_app.h"
#include <stdio.h>
#include <unistd.h>

static const char *TAG = "csv_formatter";

// 定义CSV头部的固定行宽和总字节数
#define HEADER_LINE_WIDTH 80
#define HEADER_TOTAL_LINES 8
#define HEADER_TOTAL_BYTES (HEADER_LINE_WIDTH * HEADER_TOTAL_LINES)
// 计算持续时长（Duration）值在文件中的字节偏移量
// 第7行（索引为6），" # Duration: " 之后的位置
#define DURATION_VALUE_OFFSET ((6 * HEADER_LINE_WIDTH) + strlen("# Duration: "))

static void format_header_line(char *line_buf, const char *content) { // 格式化一行内容，并用空格填充到固定宽度
    memset(line_buf, ' ', HEADER_LINE_WIDTH);                         // 清空缓冲区
    memcpy(line_buf, content, strlen(content));                       // 复制内容
    line_buf[HEADER_LINE_WIDTH - 1] = '\n';                           // 换行符
}

static int32_t csv_write_header(int fd, const data_logger_t *logger_status) {
    char header_buffer[HEADER_TOTAL_BYTES];
    char temp_buf[HEADER_LINE_WIDTH - 4]; // 临时缓冲区，-4是为了#和空格等
    // 获取时间和设备状态
    struct tm timeinfo;
    sntp_get_timeinfo(&timeinfo);
    bool notch_on = comm_get_notch();
    const int fh_map[] = {40, 100, 200};
    int lpf_index = comm_get_lpf();
    int fh = fh_map[lpf_index];
    float fl_afe = comm_get_afe_hpf() ? 0.05f : 1.5f;
    float fl_dsp = comm_get_dsp_hpf() ? 0.05f : 0.67f;
    float fl = (fl_afe > fl_dsp) ? fl_afe : fl_dsp;
    // --- 逐行构建固定宽度的头部 ---
    int offset = 0;
    // Line 1: Separator
    format_header_line(header_buffer + offset, "# --------------------");
    offset += HEADER_LINE_WIDTH;
    // Line 2: Bandwidth
    snprintf(temp_buf, sizeof(temp_buf), "# Bandwidth: %.2f~%dHz", fl, fh);
    format_header_line(header_buffer + offset, temp_buf);
    offset += HEADER_LINE_WIDTH;
    // Line 3: Notch Filter
    snprintf(temp_buf, sizeof(temp_buf), "# Notch Filter: 50Hz %s", notch_on ? "ON" : "OFF");
    format_header_line(header_buffer + offset, temp_buf);
    offset += HEADER_LINE_WIDTH;
    // Line 4: Resolution
    format_header_line(header_buffer + offset, "# Resolution: 24bit");
    offset += HEADER_LINE_WIDTH;
    // Line 5: Sample Rate
    format_header_line(header_buffer + offset, "# Sample Rate: 600SPS");
    offset += HEADER_LINE_WIDTH;
    // Line 6: Start Time
    strftime(temp_buf, sizeof(temp_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    char time_line_content[HEADER_LINE_WIDTH];
    snprintf(time_line_content, sizeof(time_line_content), "# Start Time: %s", temp_buf);
    format_header_line(header_buffer + offset, time_line_content);
    offset += HEADER_LINE_WIDTH;
    // Line 7: Duration (Placeholder)
    format_header_line(header_buffer + offset, "# Duration: calculating...");
    offset += HEADER_LINE_WIDTH;
    // Line 8: Separator
    format_header_line(header_buffer + offset, "# --------------------");
    // 一次性写入整个头部
    ssize_t bytes_written = write(fd, header_buffer, HEADER_TOTAL_BYTES);
    if (bytes_written != HEADER_TOTAL_BYTES) return -1;
    return (int32_t)bytes_written;
}

static int32_t csv_finalize_file(int fd, const data_logger_t *logger_status) {
    if (fd < 0 || logger_status == NULL) return -1;
    // 根据总样本数计算总时长
    uint32_t total_seconds = logger_status->total_samples_written / 600;
    uint32_t hours = total_seconds / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t seconds = total_seconds % 60;
    // 格式化时长字符串
    char duration_str[40];
    snprintf(duration_str, sizeof(duration_str), "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 " (total samples: %llu)", hours,
             minutes, seconds, logger_status->total_samples_written);
    // 移动文件指针到头部的 Duration 字段值的位置
    off_t seek_ret = lseek(fd, DURATION_VALUE_OFFSET, SEEK_SET);
    if (seek_ret == -1) {
        ESP_LOGE(TAG, "Failed to seek to header position for updating duration.");
        return -1;
    }
    // 写入最终的时长字符串
    ssize_t bytes_written = write(fd, duration_str, strlen(duration_str));
    if (bytes_written < 0 || (size_t)bytes_written != strlen(duration_str)) {
        ESP_LOGE(TAG, "Failed to write updated duration to header.");
        return -1;
    }
    // 文件指针移回文件末尾
    lseek(fd, 0, SEEK_END);
    return 0;
}

EXT_RAM_BSS_ATTR static char format_buffer[PSRAM_BUFFER_HALF_SIZE]; // 用于拼接字符串的本地缓冲区
static int32_t csv_write_data(int fd, const int32_t *samples, uint32_t sample_count) {
    if (fd < 0 || samples == NULL || sample_count == 0) return -1;
    int32_t buffer_offset = 0;       // 当前缓冲区已使用的长度
    int32_t total_bytes_written = 0; // 实际写入的字节数
    for (uint32_t i = 0; i < sample_count; i++) {
        int line_len =
            snprintf(format_buffer + buffer_offset, sizeof(format_buffer) - buffer_offset, "%" PRId32 "\n", samples[i]);
        if (line_len < 0) {
            ESP_LOGE(TAG, "snprintf formatting error.");
            return -1;
        }
        if (buffer_offset + line_len >= sizeof(format_buffer)) { // 检查缓冲区是否已满
            // 缓冲区已滿，先将缓冲区內的现有数据写入
            ssize_t written_now = write(fd, format_buffer, buffer_offset);
            if (written_now != buffer_offset) {
                ESP_LOGE(TAG, "File write failed while flushing full buffer.");
                return -1;
            }
            total_bytes_written += written_now;
            buffer_offset = 0; // 重置缓冲区，并将刚才没能写入的数据放入缓冲区开头
            line_len = snprintf(format_buffer, sizeof(format_buffer), "%" PRId32 "\n", samples[i]); // 再次格式化刚才那一行
        }
        buffer_offset += line_len; // 將新格式化的一行追加到缓冲区末尾
    }
    if (buffer_offset > 0) { // 写入最后剩余的数据
        ssize_t written_now = write(fd, format_buffer, buffer_offset);
        if (written_now != buffer_offset) {
            ESP_LOGE(TAG, "File write failed while flushing remaining buffer.");
            return -1;
        }
        total_bytes_written += written_now;
    }
    return total_bytes_written;
}

const data_formatter_t csv_formatter = {
    .file_extension = ".csv",
    .write_header = csv_write_header,
    .write_data = csv_write_data,
    .update_metadata = csv_finalize_file,
    .finalize_file = csv_finalize_file,
};
