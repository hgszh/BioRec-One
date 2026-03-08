#ifndef ECG_PROCESS_H_
#define ECG_PROCESS_H_

#include "mcu_fpga_comm.h"

typedef enum {
    EVT_R_WAVE_DET, // 心跳事件
    EVT_OVERLOAD,   // 过载事件
    EVT_HEARTRATE,  // 心率值更新
    EVT_REF_LINE,   // 定标线更新
} event_t;
typedef struct {
    event_t type;
    uint16_t hr;
    uint16_t rr_ms;
    uint8_t mv_ref_lenth;
    bool is_ovld;
} msg_t;

/**************************************************************************************************************/
void update_heartrate_ui(uint16_t heartrate, uint16_t rr_ms);
void update_r_wave_ui(bool current);
void update_overlod_ui(bool current);
void update_ref_line_ui(void);

/**************************************************************************************************************/
int8_t ecg_chart_agc(int32_t ecg_sample);
int8_t median3(int8_t a, int8_t b, int8_t c);

/**************************************************************************************************************/
#define MAX_WINDOW_SIZE 70
typedef struct {
    uint16_t rr_buffer[MAX_WINDOW_SIZE];
    int count;
    int start_index;
    uint16_t last_rr;
} HRV_Calculator;
void hrv_add_rr(HRV_Calculator *calc, uint16_t rr);
void hrv_clear_window(HRV_Calculator *calc);
void hrv_calculate(HRV_Calculator *calc, uint16_t *sdnn, uint16_t *rmssd);

#endif
