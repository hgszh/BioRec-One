#include "wifi_app.h"
#include "app_file_server.h"
#include "data_logger.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "lwip/ip_addr.h"
#include "rom/ets_sys.h"
#include "sntp_app.h"
#include "ssid_manager.h"
#include "wifi_configuration_ap.h"
#include "wifi_station.h"

wifi_status_t wifi_status;

static const char *TAG = "wifi";
static wifi_ap_record_t ap_info;
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_AP_START)) {
        wifi_status.is_start = true;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_status.is_valid = true;
        esp_wifi_sta_get_ap_info(&ap_info);
        snprintf(wifi_status.ssid, sizeof(wifi_status.ssid), "%s", ap_info.ssid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_status.is_valid = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(wifi_status.ip_address, sizeof(wifi_status.ip_address), IPSTR, IP2STR(&event->ip_info.ip));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

static esp_netif_t *ap_netif = NULL;
static void wifi_init_ap(const char *ssid, const char *password) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ap_netif = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

    esp_netif_ip_info_t ip;
    memset(&ip, 0, sizeof(esp_netif_ip_info_t));
    ip.ip.addr = ipaddr_addr("192.168.4.1");
    ip.gw.addr = ipaddr_addr("192.168.4.1");
    ip.netmask.addr = ipaddr_addr("255.255.255.0");
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
    snprintf((char *)wifi_config.ap.ssid, 32, "%s-%02X%02X", ssid, mac[4], mac[5]);
    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);
    snprintf((char *)wifi_config.ap.password, 64, "%s", password);
    wifi_config.ap.max_connection = 5;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.channel = 1;
    if (strlen(password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_LOGI(TAG, "wifi_init_ap finished.SSID:%s password:%s", ssid, password);
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_status.is_valid = true;
    wifi_status.is_start = true;
    snprintf(wifi_status.ssid, sizeof(wifi_status.ssid), "%s-%02X%02X", ssid, mac[4], mac[5]);
    snprintf(wifi_status.ip_address, sizeof(wifi_status.ip_address), "192.168.4.1");
}

static void wifi_deinit_ap(void) {
    esp_err_t err;
    err = esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND)
        ESP_LOGW(TAG, "Failed to unregister WIFI_EVENT handler: %s", esp_err_to_name(err));
    err = esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND)
        ESP_LOGW(TAG, "Failed to unregister IP_EVENT handler: %s", esp_err_to_name(err));
    err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) ESP_LOGW(TAG, "Failed to stop wifi: %s", esp_err_to_name(err));
    if (ap_netif) {
        esp_netif_destroy_default_wifi(ap_netif);
        ap_netif = NULL;
    }
    err = esp_wifi_deinit();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) ESP_LOGW(TAG, "Failed to deinit wifi: %s", esp_err_to_name(err));
}

/*****************************************************************************************************************/
static wifi_operating_mode_t current_mode = WIFI_STA;
static bool is_file_server_auto_start = true;

static void start_wifi_provisioning(void) {
    auto &ap = WifiConfigurationAp::GetInstance();
    ap.SetSsidPrefix("ECG-Provisioning");
    ap.Start();
    wifi_status.is_valid = true;
    wifi_status.is_start = true;
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
    snprintf(wifi_status.ssid, sizeof(wifi_status.ssid), "%s-%02X%02X", "ECG-Provisioning", mac[4], mac[5]);
    snprintf(wifi_status.ip_address, sizeof(wifi_status.ip_address), "192.168.4.1");
    wifi_status.is_provisioning = true;
}

void start_network_services(bool force_start) {
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_LOGE(TAG, "Failed to stop wifi: %s", esp_err_to_name(err));
    auto &ssid_list = SsidManager::GetInstance().GetSsidList();
    if (ssid_list.empty()) { // 如果列表为空，启动配网
        start_wifi_provisioning();
    } else {
        bool is_start = is_file_server_auto_start || force_start;
        if ((current_mode == WIFI_AP) && (is_start)) {
            wifi_init_ap("ECG-AP", "");
        } else if ((current_mode == WIFI_STA) && (is_start)) {
            ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
            ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
            WifiStation::GetInstance().Start();
        }
        if (is_start) {
            init_sntp();
            data_logger_t data_logger_status;
            data_logger_get_status_copy(&data_logger_status);
            if (data_logger_status.is_sdcard_present) start_file_server(data_logger_status.disk_path);
        } else {
            wifi_status.is_valid = false;
            wifi_status.is_start = false;
            snprintf(wifi_status.ssid, sizeof(wifi_status.ssid), "------");
            snprintf(wifi_status.ip_address, sizeof(wifi_status.ip_address), "--.--.--.--");
        }
    }
}

void stop_network_services(void) {
    stop_file_server();
    wifi_deinit_ap();
    WifiStation::GetInstance().Stop();
    WifiConfigurationAp::GetInstance().Stop();
    esp_event_loop_delete_default();
    esp_sntp_stop();
    wifi_status.is_valid = false;
    wifi_status.is_start = false;
    wifi_status.is_provisioning = false;
    snprintf(wifi_status.ssid, sizeof(wifi_status.ssid), "------");
    snprintf(wifi_status.ip_address, sizeof(wifi_status.ip_address), "--.--.--.--");
}

void restart_wifi_provisioning(void) {
    ESP_LOGI(TAG, "Restarting WiFi provisioning on-demand...");
    stop_network_services();
    esp_event_loop_create_default();
    start_wifi_provisioning();
}

wifi_operating_mode_t get_wifi_mode(void) { return current_mode; }
void set_wifi_mode(wifi_operating_mode_t mode) { current_mode = mode; }

bool get_file_server_auto_start(void) { return is_file_server_auto_start; };
void set_file_server_auto_start(bool enable) { is_file_server_auto_start = enable; }
