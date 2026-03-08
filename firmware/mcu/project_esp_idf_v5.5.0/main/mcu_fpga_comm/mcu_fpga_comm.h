#ifndef MCU_FPGA_H_
#define MCU_FPGA_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#define FPGA2MCU_SCLK GPIO_NUM_19
#define FPGA2MCU_MOSI GPIO_NUM_17
#define FPGA2MCU_MISO GPIO_NUM_18
#define FPGA2MCU_CS GPIO_NUM_8

typedef union {
    struct {
        uint8_t rwav_det : 1;    // 位0，心跳检测（只读）
        uint8_t iir_ringing : 1; // 位1，振铃指示（只读）
        uint8_t afe_ovld : 1;    // 位2，前端过载（只读）
        uint8_t afe_hpf_sel : 1; // 位3，AFE 高通1.5Hz/0.05Hz切换
        uint8_t dsp_hpf_bp : 1;  // 位4，DSP 0.67Hz高通旁路
        uint8_t notch_bp : 1;    // 位5，陷波器旁路
        uint8_t lpf : 2;         // 位6~7，低通截止频率（40/100/200Hz）
    } bits;
    uint8_t raw;
} fpga_status_reg_t;

typedef enum {
    FRAME_FLAG = 0x7e, // 帧头
    DUMMY_BYTE = 0x00, // RX时，TX传输的占位
    CMD_WREG = 0x01,   // 写寄存器
    CMD_RREG = 0x02,   // 读寄存器
    CMD_RRSQ = 0x03,   // 读RR序列
    CMD_ECGS = 0x04,   // 读ECG样本
    CMD_REST = 0x05
} spi_type_t;

typedef enum { LPF_CUTOFF_40HZ = 0, LPF_CUTOFF_100HZ = 1, LPF_CUTOFF_200HZ = 2 } lpf_cutoff_t;
typedef enum { AFE_HPF_CUTOFF_1_5HZ = 0, AFE_HPF_CUTOFF_0_05HZ = 1 } afe_hpf_cutoff_t;
typedef enum { DSP_HPF_CUTOFF_0_67HZ = 0, DSP_HPF_CUTOFF_0_05HZ = 1 } dsp_hpf_cutoff_t;

extern QueueHandle_t event_queue;
// 用于发送给LVGL图表的数据 (int8_t)
extern StreamBufferHandle_t ecg_chart_stream_buffer;
// 用于写入SD卡的原始ECG数据 (int32_t)
extern StreamBufferHandle_t raw_ecg_stream_buffer;

void init_fpga_comm_task(void);
void reset_fpga(void);
void comm_set_lpf(lpf_cutoff_t new_cutoff);
void comm_set_afe_hpf(afe_hpf_cutoff_t new_cutoff);
void comm_set_dsp_hpf(dsp_hpf_cutoff_t new_cutoff);
void comm_set_notch(bool en);
lpf_cutoff_t comm_get_lpf(void);
afe_hpf_cutoff_t comm_get_afe_hpf(void);
dsp_hpf_cutoff_t comm_get_dsp_hpf(void);
bool comm_get_notch(void);

#endif
