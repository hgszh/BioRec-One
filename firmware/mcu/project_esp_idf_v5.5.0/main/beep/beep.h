#ifndef _BEEP_H_
#define _BEEP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define BEEP_IO (48)
void init_beep_task(void);
void send_buzzer_cmd(uint16_t freq, uint8_t duty, uint16_t duration_ms);
void poweron_beep_melody(void);
void btn_beep(void);
void back_home_long_beep(void);
void ntp_sync_beep(void);
void r_wave_beep(void);
void countdown_beep(void);

void set_system_beep(bool en);
void set_heartbeat_beep(bool en);
bool get_system_beep(void);
bool get_heartbeat_beep(void);

#ifdef __cplusplus
}
#endif

#endif