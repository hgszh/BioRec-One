#ifndef __WIFI_APP_H__
#define __WIFI_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { WIFI_AP, WIFI_STA } wifi_operating_mode_t;
typedef struct wifi_status {
    char ssid[33];
    char ip_address[30];
    bool is_valid;
    bool is_start;
    bool is_provisioning;
} wifi_status_t;
extern wifi_status_t wifi_status;

void start_network_services(bool force_start);
void stop_network_services(void);
void restart_wifi_provisioning(void);
wifi_operating_mode_t get_wifi_mode(void);
void set_wifi_mode(wifi_operating_mode_t mode);
bool get_file_server_auto_start(void);
void set_file_server_auto_start(bool enable);

#ifdef __cplusplus
}
#endif

#endif // __WIFI_APP_H__
