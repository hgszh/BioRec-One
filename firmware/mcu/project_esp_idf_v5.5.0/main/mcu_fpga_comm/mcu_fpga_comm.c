#include "mcu_fpga_comm.h"
#include "crc.h"
#include "data_logger.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "ecg_process.h"
#include "esp_log.h"
#include "nvs_app.h"
#include <math.h>
#include <string.h>

static SemaphoreHandle_t fpga_reg_mutex;
static spi_device_handle_t spi_master;
static fpga_status_reg_t fpga_reg;
/**************************************************************************************************************/
static void mcu_fpga_spi_init(void) {
    spi_bus_config_t buscfg = {
        .miso_io_num = FPGA2MCU_MISO,
        .mosi_io_num = FPGA2MCU_MOSI,
        .sclk_io_num = FPGA2MCU_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 24,
        .isr_cpu_id = 0,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = FPGA2MCU_CS,
        .queue_size = 12,
    };
    gpio_pullup_en(FPGA2MCU_CS);
    gpio_pullup_en(FPGA2MCU_MOSI);
    gpio_pullup_en(FPGA2MCU_SCLK);
    spi_bus_add_device(SPI2_HOST, &devcfg, &spi_master);
}

/**************************************************************************************************************/
static void spi_read_write_byte(uint8_t *tx_buffer, uint8_t *rx_buffer) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = tx_buffer;
    t.rx_buffer = rx_buffer;
    spi_device_transmit(spi_master, &t);
}
static bool read_fpga_register(uint8_t reg, uint8_t *data) {
    uint8_t tx_byte = 0, rx_byte = 0, crc = 0, data_tmp = 0;
    tx_byte = FRAME_FLAG;
    spi_read_write_byte(&tx_byte, &rx_byte);
    tx_byte = CMD_RREG;
    spi_read_write_byte(&tx_byte, &rx_byte);
    crc = crc8_compute(crc, tx_byte);
    tx_byte = reg;
    spi_read_write_byte(&tx_byte, &rx_byte);
    crc = crc8_compute(crc, tx_byte);
    tx_byte = DUMMY_BYTE; // 接收命令的CRC值
    spi_read_write_byte(&tx_byte, &rx_byte);
    if (rx_byte != crc) {
        ESP_LOGE("read_fpga_register", "cmd crc mismatch calc:%u recv:%u", crc, rx_byte);
        return false;
    }
    tx_byte = DUMMY_BYTE; // 接收数据
    spi_read_write_byte(&tx_byte, &data_tmp);
    crc = 0;
    crc = crc8_compute(crc, data_tmp);
    tx_byte = DUMMY_BYTE; // 接收数据的CRC值
    spi_read_write_byte(&tx_byte, &rx_byte);
    if (rx_byte != crc) {
        ESP_LOGE("read_fpga_register", "data crc mismatch calc:%u recv:%u", crc, rx_byte);
        return false;
    }
    *data = data_tmp;
    return true;
}
static bool write_fpga_register(uint8_t reg, uint8_t data) {
    uint8_t tx_byte = 0, rx_byte = 0, crc = 0;
    tx_byte = FRAME_FLAG;
    spi_read_write_byte(&tx_byte, &rx_byte);
    tx_byte = CMD_WREG;
    spi_read_write_byte(&tx_byte, &rx_byte);
    crc = crc8_compute(crc, tx_byte);
    tx_byte = reg;
    spi_read_write_byte(&tx_byte, &rx_byte);
    crc = crc8_compute(crc, tx_byte);
    tx_byte = DUMMY_BYTE; // 接收命令的CRC值
    spi_read_write_byte(&tx_byte, &rx_byte);
    if (rx_byte != crc) {
        ESP_LOGE("write_fpga_register", "cmd crc mismatch calc:%u recv:%u", crc, rx_byte);
        return false;
    }
    tx_byte = data; // 写入数据
    spi_read_write_byte(&tx_byte, &rx_byte);
    crc = 0;
    crc = crc8_compute(crc, data);
    tx_byte = DUMMY_BYTE; // 接收数据的CRC值
    spi_read_write_byte(&tx_byte, &rx_byte);
    if (rx_byte != crc) {
        ESP_LOGE("write_fpga_register", "data crc mismatch calc:%u recv:%u", crc, rx_byte);
        return false;
    }
    return true;
}
void reset_fpga(void) {
    xSemaphoreTake(fpga_reg_mutex, portMAX_DELAY);
    // 记录复位前控制位的值
    uint8_t last_val = fpga_reg.raw;
    // 发送复位命令
    uint8_t tx_byte = 0;
    tx_byte = FRAME_FLAG;
    spi_read_write_byte(&tx_byte, NULL);
    tx_byte = CMD_REST;
    spi_read_write_byte(&tx_byte, NULL);
    // 将复位后的控制位恢复成复位前
    write_fpga_register(0x00, last_val);
    xSemaphoreGive(fpga_reg_mutex);
}

/**************************************************************************************************************/
static bool get_rr_fifo_count(uint8_t *fifo_count) {
    if (read_fpga_register(0x02, fifo_count)) return true;
    ESP_LOGE("get_rr_fifo_count", "error");
    return false;
}

static bool get_rr_interval_sequence(uint8_t fifo_count, uint16_t *rr_batch) {
    uint8_t tx_byte = 0, rx_byte = 0, crc = 0, byte_h = 0, byte_l = 0;
    uint16_t rr_interval = 0;
    uint16_t heart_rate = 0;
    tx_byte = FRAME_FLAG;
    spi_read_write_byte(&tx_byte, &rx_byte);
    tx_byte = CMD_RRSQ;
    spi_read_write_byte(&tx_byte, &rx_byte);
    crc = crc8_compute(crc, tx_byte);
    tx_byte = fifo_count;
    spi_read_write_byte(&tx_byte, &rx_byte);
    crc = crc8_compute(crc, tx_byte);
    tx_byte = DUMMY_BYTE;
    spi_read_write_byte(&tx_byte, &rx_byte);
    if (crc != rx_byte) {
        ESP_LOGE("get_rr_interval_sequence", "cmd crc mismatch, calc:%u recv:%u", crc, rx_byte);
        return false;
    }
    for (uint8_t i = 0; i < fifo_count; i++) {
        tx_byte = DUMMY_BYTE;
        spi_read_write_byte(&tx_byte, &byte_h);
        tx_byte = DUMMY_BYTE;
        spi_read_write_byte(&tx_byte, &byte_l);
        rr_interval = ((uint16_t)byte_h << 8) | (uint16_t)byte_l;
        rr_batch[i] = rr_interval;
        if ((rr_interval > 200) && (rr_interval < 3000)) {
            heart_rate = round(60000.0 / rr_interval);
            update_heartrate_ui(heart_rate, rr_interval);
        }
    }
    return true;
}

/**************************************************************************************************************/
static bool get_ecg_fifo_count(uint8_t *fifo_count) {
    if (read_fpga_register(0x01, fifo_count)) return true;
    ESP_LOGE("get_ecg_fifo_count", "error");
    return false;
}

static void communication_watch_dog(int32_t ecg_sample) {
    static uint8_t consecutive_count = 0;
    static int32_t last_ecg_sample;
    if (ecg_sample == last_ecg_sample) consecutive_count++;
    else consecutive_count = 0;
    last_ecg_sample = ecg_sample;
    if (consecutive_count >= 50) {
        ESP_LOGW("get_ecg_sample", "Communication lost! resetting FPGA...");
        reset_fpga();
        consecutive_count = 0;
    }
}

StreamBufferHandle_t ecg_chart_stream_buffer;
StreamBufferHandle_t raw_ecg_stream_buffer;
static bool get_ecg_sample(uint8_t fifo_count, int32_t *ecg_batch) {
    uint8_t tx_byte = 0, rx_byte = 0, crc = 0, byte_h = 0, byte_m = 0, byte_l = 0;
    int32_t ecg_sample = 0;
    static int8_t window[3];
    static uint8_t win_cnt = 0;
    tx_byte = FRAME_FLAG;
    spi_read_write_byte(&tx_byte, &rx_byte);
    tx_byte = CMD_ECGS;
    spi_read_write_byte(&tx_byte, &rx_byte);
    crc = crc8_compute(crc, tx_byte);
    tx_byte = fifo_count;
    spi_read_write_byte(&tx_byte, &rx_byte);
    crc = crc8_compute(crc, tx_byte);
    tx_byte = DUMMY_BYTE;
    spi_read_write_byte(&tx_byte, &rx_byte);
    if (crc != rx_byte) {
        ESP_LOGE("get_ecg_sample", "cmd crc mismatch, calc:%u recv:%u", crc, rx_byte);
        return false;
    }
    for (uint8_t i = 0; i < fifo_count; i++) {
        tx_byte = DUMMY_BYTE;
        spi_read_write_byte(&tx_byte, &byte_h);
        tx_byte = DUMMY_BYTE;
        spi_read_write_byte(&tx_byte, &byte_m);
        tx_byte = DUMMY_BYTE;
        spi_read_write_byte(&tx_byte, &byte_l);
        ecg_sample = ((int32_t)byte_h << 16) | ((int32_t)byte_m << 8) | (int32_t)byte_l;
        if (byte_h & 0x80) ecg_sample = ecg_sample | 0xFF000000; // 符号位扩展
        else ecg_sample = ecg_sample & 0xFFFFFF;
        ecg_batch[i] = ecg_sample;
        communication_watch_dog(ecg_sample);
        window[win_cnt] = ecg_chart_agc(ecg_sample); // 把24位数据自适应偏移缩放到屏幕高度
        win_cnt = win_cnt + 1;
        if (win_cnt == 3) { // 每3个点取中位数发送更新图表
            win_cnt = 0;
            int8_t med = median3(window[0], window[1], window[2]);
            xStreamBufferSend(ecg_chart_stream_buffer, &med, sizeof(med), 0);
        }
    }
    return true;
}

/**************************************************************************************************************/
// 切换低通截止频率
void comm_set_lpf(lpf_cutoff_t new_cutoff) {
    xSemaphoreTake(fpga_reg_mutex, portMAX_DELAY);
    fpga_reg.bits.lpf = (uint8_t)new_cutoff & 0x03; // 只保留2位
    xSemaphoreGive(fpga_reg_mutex);
}
// 切换AFE高通截止频率
void comm_set_afe_hpf(afe_hpf_cutoff_t new_cutoff) {
    xSemaphoreTake(fpga_reg_mutex, portMAX_DELAY);
    fpga_reg.bits.afe_hpf_sel = (uint8_t)new_cutoff & 0x01;
    xSemaphoreGive(fpga_reg_mutex);
}
// 切换DSP高通截止频率
void comm_set_dsp_hpf(dsp_hpf_cutoff_t new_cutoff) {
    xSemaphoreTake(fpga_reg_mutex, portMAX_DELAY);
    fpga_reg.bits.dsp_hpf_bp = (uint8_t)new_cutoff & 0x01;
    xSemaphoreGive(fpga_reg_mutex);
}
// 陷波器开关
void comm_set_notch(bool en) {
    xSemaphoreTake(fpga_reg_mutex, portMAX_DELAY);
    fpga_reg.bits.notch_bp = !en;
    xSemaphoreGive(fpga_reg_mutex);
}
/******************************************************/
// 获取低通截止频率
lpf_cutoff_t comm_get_lpf(void) {
    lpf_cutoff_t local_cutoff;
    xSemaphoreTake(fpga_reg_mutex, portMAX_DELAY);
    local_cutoff = fpga_reg.bits.lpf;
    xSemaphoreGive(fpga_reg_mutex);
    return local_cutoff;
}
// 获取AFE高通截止频率
afe_hpf_cutoff_t comm_get_afe_hpf(void) {
    afe_hpf_cutoff_t local_cutoff;
    xSemaphoreTake(fpga_reg_mutex, portMAX_DELAY);
    local_cutoff = fpga_reg.bits.afe_hpf_sel;
    xSemaphoreGive(fpga_reg_mutex);
    return local_cutoff;
}
// 获取DSP高通截止频率
dsp_hpf_cutoff_t comm_get_dsp_hpf(void) {
    dsp_hpf_cutoff_t local_cutoff;
    xSemaphoreTake(fpga_reg_mutex, portMAX_DELAY);
    local_cutoff = fpga_reg.bits.dsp_hpf_bp;
    xSemaphoreGive(fpga_reg_mutex);
    return local_cutoff;
}
// 获取陷波器状态
bool comm_get_notch(void) {
    bool local_status;
    xSemaphoreTake(fpga_reg_mutex, portMAX_DELAY);
    local_status = !fpga_reg.bits.notch_bp;
    xSemaphoreGive(fpga_reg_mutex);
    return local_status;
}
/**************************************************************************************************************/
QueueHandle_t event_queue;
static void fpga_comm_task(void *arg) {
    uint8_t retry = 0;
    uint8_t fifo_count = 0;
    uint16_t rr_batch[8] = {0};
    int32_t ecg_batch[64] = {0};
    uint8_t current_val = 0;
    uint8_t last_written_val = 0;
    reset_fpga();
    while (1) {
        /*******************************************/
        // 读取MCU结构体控制位（高5位）
        xSemaphoreTake(fpga_reg_mutex, portMAX_DELAY);
        current_val = fpga_reg.raw & 0xF8;
        xSemaphoreGive(fpga_reg_mutex);
        // 若控制位变化，MCU对FPGA写入，更新寄存器
        if ((last_written_val & 0xF8) != current_val) {
            for (retry = 3; retry > 0; retry--) {
                if (write_fpga_register(0x00, current_val)) {
                    last_written_val = current_val;
                    break;
                }
            }
            if (retry == 0) reset_fpga();
        }
        /*******************************************/
        // 回读FPGA寄存器，更新MCU结构体和UI
        else {
            for (retry = 3; retry > 0; retry--) {
                if (read_fpga_register(0x00, &current_val)) {
                    xSemaphoreTake(fpga_reg_mutex, portMAX_DELAY);
                    uint8_t fpga_status_bits = current_val & 0x07;
                    fpga_reg.raw = (fpga_reg.raw & 0xF8) | fpga_status_bits;
                    bool r_wave_current = fpga_reg.bits.rwav_det;
                    bool afe_ovld_current = fpga_reg.bits.afe_ovld;
                    xSemaphoreGive(fpga_reg_mutex);
                    update_r_wave_ui(r_wave_current);
                    update_overlod_ui(afe_ovld_current);
                    break;
                }
            }
            if (retry == 0) reset_fpga();
        }
        /*******************************************/
        for (retry = 3; retry > 0; retry--) {
            if (get_rr_fifo_count(&fifo_count)) break;
        }
        if (retry == 0) reset_fpga();
        if (fifo_count) {
            for (retry = 3; retry > 0; retry--) {
                if (get_rr_interval_sequence(fifo_count, rr_batch)) break;
            }
        }
        if (retry == 0) reset_fpga();
        /*******************************************/
        for (retry = 3; retry > 0; retry--) {
            if (get_ecg_fifo_count(&fifo_count)) break;
        }
        if (retry == 0) reset_fpga();
        if (fifo_count) {
            for (retry = 3; retry > 0; retry--) {
                if (get_ecg_sample(fifo_count, ecg_batch)) break;
            }
            if (retry == 0) reset_fpga();
            if (g_data_logger.is_recording &&
                !g_data_logger.is_paused) { // 将原始的24-bit数据(作为int32_t)发送到data_logger缓冲区
                xStreamBufferSend(raw_ecg_stream_buffer, &ecg_batch, fifo_count * sizeof(int32_t), 0);
            }
        }
        update_ref_line_ui();
        /*******************************************/
        vTaskDelay(40);
    }
}

void init_fpga_comm_task(void) {
    event_queue = xQueueCreate(30, sizeof(msg_t));
    fpga_reg_mutex = xSemaphoreCreateMutex();
    ecg_chart_stream_buffer = xStreamBufferCreate(128 * sizeof(int8_t), 1);
    raw_ecg_stream_buffer = xStreamBufferCreate(128 * sizeof(int32_t), 1);
    mcu_fpga_spi_init();
    xTaskCreatePinnedToCore(fpga_comm_task, "fpga_comm", 4096, NULL, 11, NULL, 0);
}
