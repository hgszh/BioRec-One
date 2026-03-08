#include "data_formatter.h"
#include "esp_log.h"
#include "mcu_fpga_comm.h"
#include "sntp_app.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *TAG = "bdf_formatter";

#define BDF_HEADER_TOTAL_BYTES 512 // 文件头总字节数 (256基础 + 1通道 * 256)
#define SAMPLING_FREQUENCY_SPS 600 // 采样频率
#define BDF_NUM_RECORDS_OFFSET 236 // "number of data records"字段在文件头中的字节偏移量
#define PHYSICAL_MAX_UV 2036       // 物理最大值，单位uV
#define PHYSICAL_MIN_UV -2036      // 物理最小值，单位uV

static void format_bdf_field(char *dest, const char *src, size_t len) {
    memset(dest, ' ', len);
    size_t src_len = strlen(src);
    memcpy(dest, src, src_len < len ? src_len : len);
}

// see:
// https://www.teuniz.net/edfbrowser/bdfplus%20format%20description.html
// https://www.biosemi.com/faq/file_format.htm
// https://www.edfplus.info/specs/index.html
static int32_t bdf_write_header(int fd, const data_logger_t *logger_status) {
    const int header_total_bytes = BDF_HEADER_TOTAL_BYTES;
    char *header = (char *)malloc(header_total_bytes);
    if (!header) {
        ESP_LOGE(TAG, "Failed to allocate memory for header");
        return -1;
    }
    memset(header, ' ', header_total_bytes);
    // --- 主文件头 (256字节) ---
    // version of this data format
    header[0] = 255;
    memcpy(&header[1], "BIOSEMI", 7);
    // local patient identification
    format_bdf_field(&header[8], "Patient: X", 80);
    // local recording identification
    struct tm timeinfo;
    sntp_get_timeinfo(&timeinfo);
    char rec_info[80];
    strftime(rec_info, sizeof(rec_info), "Startdate %d-%b-%Y ECG_recording", &timeinfo);
    format_bdf_field(&header[88], rec_info, 80);
    // startdate of recording (dd.mm.yy)
    char temp_buf[20];
    strftime(temp_buf, sizeof(temp_buf), "%d.%m.%y", &timeinfo);
    format_bdf_field(&header[168], temp_buf, 8);
    // starttime of recording (hh.mm.ss)
    strftime(temp_buf, sizeof(temp_buf), "%H.%M.%S", &timeinfo);
    format_bdf_field(&header[176], temp_buf, 8);
    // number of bytes in header record
    snprintf(temp_buf, sizeof(temp_buf), "%d", header_total_bytes);
    format_bdf_field(&header[184], temp_buf, 8);
    // reserved
    // format_bdf_field(&header[192], "BDF+C", 44);
    format_bdf_field(&header[192], "", 44);
    // number of data records
    format_bdf_field(&header[236], "-1", 8);
    // duration of a data record, in seconds
    format_bdf_field(&header[244], "1", 8);
    // number of signals (ns) in data record
    snprintf(temp_buf, sizeof(temp_buf), "%d", 1);
    format_bdf_field(&header[252], temp_buf, 4);
    // --- 通道信息 (1 * 256字节) ---
    // label (e.g. EEG Fpz-Cz or Body temp)
    int offset = 256;
    format_bdf_field(&header[offset], "ECG", 16);
    // transducer type (e.g. AgAgCl electrode)
    offset += 16;
    format_bdf_field(&header[offset], "Dry electrode", 80);
    offset += 80;
    // physical dimension (e.g. uV or degreeC)
    format_bdf_field(&header[offset], "uV", 8);
    offset += 8;
    // physical minimum
    snprintf(temp_buf, sizeof(temp_buf), "%d", PHYSICAL_MIN_UV);
    format_bdf_field(&header[offset], temp_buf, 8);
    offset += 8;
    // physical maximum
    snprintf(temp_buf, sizeof(temp_buf), "%d", PHYSICAL_MAX_UV);
    format_bdf_field(&header[offset], temp_buf, 8);
    offset += 8;
    // digital minimum
    const int32_t dig_min = -8388608;
    snprintf(temp_buf, sizeof(temp_buf), "%" PRId32, dig_min);
    format_bdf_field(&header[offset], temp_buf, 8);
    offset += 8;
    // digital maximum
    const int32_t dig_max = 8388607;
    snprintf(temp_buf, sizeof(temp_buf), "%" PRId32, dig_max);
    format_bdf_field(&header[offset], temp_buf, 8);
    offset += 8;
    // prefiltering (e.g. HP:0.1Hz LP:75Hz)
    bool notch_on = comm_get_notch();
    const int fh_map[] = {40, 100, 200};
    int lpf_index = comm_get_lpf();
    int fh = fh_map[lpf_index];
    float fl_afe = comm_get_afe_hpf() ? 0.05f : 1.5f;
    float fl_dsp = comm_get_dsp_hpf() ? 0.05f : 0.67f;
    float fl = (fl_afe > fl_dsp) ? fl_afe : fl_dsp;
    char prefilter_buf[80];
    snprintf(prefilter_buf, sizeof(prefilter_buf), "HP:%.2fHz; LP:%dHz; Notch:50Hz %s", fl, fh, notch_on ? "ON" : "OFF");
    format_bdf_field(&header[offset], prefilter_buf, 80);
    offset += 80;
    // Number of samples in each data record(Sample-rate if Duration of data record = "1")
    snprintf(temp_buf, sizeof(temp_buf), "%d", SAMPLING_FREQUENCY_SPS);
    format_bdf_field(&header[offset], temp_buf, 8);
    offset += 8;
    // reserved
    format_bdf_field(&header[offset], "", 32);
    offset += 32;
    // --------------------------------------
    ssize_t bytes_written = write(fd, header, header_total_bytes);
    free(header);
    if (bytes_written != header_total_bytes) {
        ESP_LOGE(TAG, "Failed to write BDF header, wrote %zd bytes", bytes_written);
        return -1;
    }
    return (int32_t)bytes_written;
}

// 将32位整型样本数据转换为24位小端格式并写入文件
EXT_RAM_BSS_ATTR static uint8_t format_buffer[PSRAM_BUFFER_HALF_SIZE];
static int32_t bdf_write_data(int fd, const int32_t *samples, uint32_t sample_count) {
    if (fd < 0 || samples == NULL || sample_count == 0) return -1;
    const size_t converted_size = sample_count * 3;
    for (uint32_t i = 0; i < sample_count; i++) {
        int32_t sample = samples[i];
        format_buffer[i * 3 + 0] = (sample >> 0) & 0xFF;
        format_buffer[i * 3 + 1] = (sample >> 8) & 0xFF;
        format_buffer[i * 3 + 2] = (sample >> 16) & 0xFF;
    }
    ssize_t bytes_written = write(fd, format_buffer, converted_size);
    if (bytes_written < 0 || (size_t)bytes_written != converted_size) {
        ESP_LOGE(TAG, "File write failed for BDF data.");
        return -1;
    }
    return (int32_t)bytes_written;
}

// 实时回填总记录数，避免意外断电文件结构不完整
static int32_t bdf_update_metadata(int fd, const data_logger_t *logger_status) {
    if (fd < 0 || logger_status == NULL) return -1;
    // 仅计算和回填总记录数，不做任何其他操作
    uint32_t total_records = logger_status->total_samples_written / SAMPLING_FREQUENCY_SPS;
    char num_records_str[9];
    snprintf(num_records_str, sizeof(num_records_str), "%-8" PRIu32, total_records);
    if (lseek(fd, BDF_NUM_RECORDS_OFFSET, SEEK_SET) == -1) return -1;
    if (write(fd, num_records_str, 8) != 8) return -1;
    lseek(fd, 0, SEEK_END); // 指针移回末尾
    return 0;
}

static int32_t bdf_finalize_file(int fd, const data_logger_t *logger_status) {
    if (fd < 0 || logger_status == NULL) {
        return -1;
    }
    uint64_t total_samples_written = logger_status->total_samples_written;
    uint32_t total_records = total_samples_written / SAMPLING_FREQUENCY_SPS;
    uint32_t remainder_samples = total_samples_written % SAMPLING_FREQUENCY_SPS;
    // 如果存在不完整的记录块（即总样本数不是600的整数倍），则需要补0
    if (remainder_samples > 0) {
        // 计算需要补充多少个样本点（补足成一个完整的记录块）
        uint32_t samples_to_pad = SAMPLING_FREQUENCY_SPS - remainder_samples;
        size_t bytes_to_pad = samples_to_pad * 3; // 每个样本点3个字节
        // 分配一个填满0的缓冲区
        uint8_t *padding_buffer = (uint8_t *)calloc(bytes_to_pad, 1);
        if (padding_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for padding buffer");
            return -1;
        }
        // 将0写入文件末尾
        ssize_t written_bytes = write(fd, padding_buffer, bytes_to_pad);
        free(padding_buffer);
        if ((size_t)written_bytes != bytes_to_pad) {
            ESP_LOGE(TAG, "Failed to write padding bytes to file.");
            return -1;
        }
        // 因为补足了一个记录块，所以总记录数要加1
        total_records++;
    }
    // 移动文件指针并回填最终的总记录数
    char num_records_str[9];
    snprintf(num_records_str, sizeof(num_records_str), "%-8" PRIu32, total_records);
    if (lseek(fd, BDF_NUM_RECORDS_OFFSET, SEEK_SET) == -1) {
        ESP_LOGE(TAG, "Failed to seek to header position for updating num_records.");
        return -1;
    }
    if (write(fd, num_records_str, 8) != 8) {
        ESP_LOGE(TAG, "Failed to write updated num_records to header.");
        return -1;
    }
    lseek(fd, 0, SEEK_END);
    return 0;
}

const data_formatter_t bdf_formatter = {
    .file_extension = ".bdf",
    .write_header = bdf_write_header,
    .write_data = bdf_write_data,
    .update_metadata = bdf_update_metadata,
    .finalize_file = bdf_finalize_file,
};
