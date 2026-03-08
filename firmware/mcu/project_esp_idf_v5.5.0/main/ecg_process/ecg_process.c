#include "ecg_process.h"
#include <math.h>
#include <string.h>

/**************************************************************************************************************/
void update_heartrate_ui(uint16_t heartrate, uint16_t rr_ms) {
    msg_t evt = {.type = EVT_HEARTRATE, .hr = heartrate, .rr_ms = rr_ms};
    xQueueSend(event_queue, &evt, 0);
}

/**************************************************************************************************************/
void update_r_wave_ui(bool current) {
    static bool last_r_state = false;
    static uint8_t no_r_wave_count = 0;
    if (!last_r_state && current) {
        msg_t msg = {.type = EVT_R_WAVE_DET};
        xQueueSend(event_queue, &msg, 0);
        no_r_wave_count = 0;
    } else { // 若超时每检测到心跳，就归零显示的心率值
        no_r_wave_count++;
        if (no_r_wave_count >= 150) {
            msg_t evt = {.type = EVT_HEARTRATE, .hr = 0, .rr_ms = 0};
            xQueueSend(event_queue, &evt, 0);
            no_r_wave_count = 0;
        }
    }
    last_r_state = current;
}

void update_overlod_ui(bool current) {
    static bool last_overlod_state = false;
    if (last_overlod_state != current) {
        msg_t msg = {.type = EVT_OVERLOAD, .is_ovld = current};
        xQueueSend(event_queue, &msg, 0);
    }
    last_overlod_state = current;
}

/**************************************************************************************************************/
// clang-format off
 int8_t median3(int8_t a, int8_t b, int8_t c) {
    if(a > b) { int8_t t = a; a = b; b = t; }
    if(b > c) { int8_t t = b; b = c; c = t; }
    if(a > b) { int8_t t = a; a = b; b = t; }
    return b;
} // clang-format on

/**************************************************************************************************************/
#define OFFSET_FAST_SHIFT 9
#define OFFSET_SLOW_SHIFT 14
#define OFFSET_THRESH 200
// GAIN_POS越大，增益下降越慢
#define GAIN_POS_FAST_SHIFT 5 // 幅度突增时，快速降低增益
#define GAIN_POS_SLOW_SHIFT 7 // 幅度略有增大时，慢速降低增益
// GAIN_NEG越大，增益上升越慢
#define GAIN_NEG_FAST_SHIFT 12 // 幅度锐减时，快速增加增益
#define GAIN_NEG_SLOW_SHIFT 13 // 幅度略微减小时，慢速增加增益
#define GAIN_THRESH 1200000
#define GAIN_HEADROOM 1500000
#define APPLY_AGC_GAIN(value, gain_denom) (((int64_t)(value) * 128) / ((gain_denom) + GAIN_HEADROOM))
static int32_t current_gain_denom = 4194304;
int8_t ecg_chart_agc(int32_t ecg_sample) {
    static int32_t current_offset = 0;
    int32_t offset_err = ecg_sample - current_offset;
    int32_t offset_shift = (labs(offset_err) < OFFSET_THRESH) ? OFFSET_SLOW_SHIFT : OFFSET_FAST_SHIFT;
    current_offset += offset_err >> offset_shift;
    int32_t offset_removed = ecg_sample - current_offset;
    int32_t gain_err = labs(offset_removed) - current_gain_denom;
    if (gain_err > 0) {
        int32_t shift = (gain_err < GAIN_THRESH) ? GAIN_POS_SLOW_SHIFT : GAIN_POS_FAST_SHIFT;
        current_gain_denom += gain_err >> shift;
    } else if (gain_err < 0) {
        int32_t neg_err = -gain_err;
        int32_t shift = (neg_err < GAIN_THRESH) ? GAIN_NEG_SLOW_SHIFT : GAIN_NEG_FAST_SHIFT;
        current_gain_denom -= neg_err >> shift;
    }
    int32_t tmp = APPLY_AGC_GAIN(offset_removed, current_gain_denom);
    if (tmp > 127) tmp = 127;
    else if (tmp < -128) tmp = -128;
    return (int8_t)tmp;
}

#define AFE_GAIN (1228)
#define ADC_VREF_MILLIVOL (2500)
#define ADC_TOTAL_VOLTAGE_RANGE (2 * ADC_VREF_MILLIVOL)
#define ADC_CODE_RANGE (1 << 24)
#define CHART_WAVEFORM_MAX_HEIGHT_PX (1 << 8)
#define REF_LINE_MAX_HEIGHT_PX (170)
static inline uint16_t calculate_mv_to_display_pixels(int32_t gain_denom) {
    const uint64_t adc_codes_per_millivol = ((uint64_t)AFE_GAIN * ADC_CODE_RANGE) / ADC_TOTAL_VOLTAGE_RANGE;
    const uint64_t agc_applied_value = (adc_codes_per_millivol * 128) / (gain_denom + GAIN_HEADROOM);
    return (uint16_t)agc_applied_value;
}
#define FILTER_POINTS 40
static uint8_t get_mv_ref_line_lenth(void) {
    static uint16_t history[FILTER_POINTS] = {0};
    static uint32_t sum = 0;
    static uint8_t index = 0;
    uint16_t current_lenth =
        calculate_mv_to_display_pixels(current_gain_denom) * REF_LINE_MAX_HEIGHT_PX / CHART_WAVEFORM_MAX_HEIGHT_PX;
    sum = sum - history[index] + current_lenth;
    history[index] = current_lenth;
    index = (index + 1) % FILTER_POINTS;
    uint16_t average_lenth = sum / FILTER_POINTS;

    return (average_lenth > REF_LINE_MAX_HEIGHT_PX) ? REF_LINE_MAX_HEIGHT_PX : (uint8_t)average_lenth;
}

void update_ref_line_ui(void) {
    msg_t evt = {.type = EVT_REF_LINE, .mv_ref_lenth = get_mv_ref_line_lenth()};
    xQueueSend(event_queue, &evt, 0);
}

/**************************************************************************************************************/
void hrv_add_rr(HRV_Calculator *calc, uint16_t rr) {
    if ((rr > 0) && (rr < 2000) && (abs(rr - calc->last_rr) < (calc->last_rr / 2))) {
        if (calc->count < MAX_WINDOW_SIZE) {
            int index = (calc->start_index + calc->count) % MAX_WINDOW_SIZE;
            calc->rr_buffer[index] = rr;
            calc->count++;
        } else {
            calc->rr_buffer[calc->start_index] = rr;
            calc->start_index = (calc->start_index + 1) % MAX_WINDOW_SIZE;
        }
    }
    calc->last_rr = rr;
}
void hrv_clear_window(HRV_Calculator *calc) { memset(calc, 0, sizeof(HRV_Calculator)); }
void hrv_calculate(HRV_Calculator *calc, uint16_t *sdnn, uint16_t *rmssd) {
    int n = calc->count;
    *sdnn = 0;
    *rmssd = 0;
    if (n < 2) return;
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        int idx = (calc->start_index + i) % MAX_WINDOW_SIZE;
        sum += calc->rr_buffer[idx];
    }
    float mean = sum / n;
    float sum_sq_diff = 0.0f;
    float sum_sq_delta = 0.0f;
    for (int i = 0; i < n; i++) {
        int idx = (calc->start_index + i) % MAX_WINDOW_SIZE;
        float diff = calc->rr_buffer[idx] - mean;
        sum_sq_diff += diff * diff;
        if (i < n - 1) {
            int next_idx = (calc->start_index + i + 1) % MAX_WINDOW_SIZE;
            float delta = (float)calc->rr_buffer[next_idx] - calc->rr_buffer[idx];
            sum_sq_delta += delta * delta;
        }
    }
    *sdnn = (uint16_t)round(sqrt(sum_sq_diff / n));
    *rmssd = (uint16_t)round(sqrt(sum_sq_delta / (n - 1)));
}
