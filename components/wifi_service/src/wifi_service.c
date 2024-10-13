#include "wifi_service.h"

#include "device_common.h"
#include <stdio.h>
#include <string.h>


#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include "sdkconfig.h"
#include "esp_mac.h"
#include "toolbox.h"
#include "device_common.h"
#include "setting_server.h"
#include "device_macro.h"

#include "string.h"


#define MIN_WIFI_PWD_LEN  8
#define MIN_WIFI_SSID_LEN 1 


wifi_mode_t wifi_mode;

static esp_netif_t *netif;
static wifi_config_t wifi_sta_config;

static wifi_config_t wifi_ap_config = {
        .ap.password = CONFIG_WIFI_AP_PASSWORD,
        .ap.ssid = CONFIG_WIFI_AP_SSID,
        .ap.max_connection = CONFIG_MAX_STA_CONN,
        .ap.authmode = WIFI_AUTH_WPA_WPA2_PSK,
        .ap.channel = 1,
        .ap.pmf_cfg.required = false
};



static void sta_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    static int retry_num;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        retry_num = 0;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if(retry_num < 5){
            esp_wifi_connect();
            ++retry_num;
        } else {
            wifi_event_sta_disconnected_t *event_sta_disconnected = (wifi_event_sta_disconnected_t *) event_data;
            if(event_sta_disconnected->reason == WIFI_REASON_NO_AP_FOUND
                        || event_sta_disconnected->reason == WIFI_REASON_HANDSHAKE_TIMEOUT){
                device_set_state(BIT_ERR_SSID_NOT_FOUND);
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        retry_num = 0;
        device_set_state(BIT_IS_STA_CONNECTION);
        device_clear_state(BIT_ERR_SSID_NOT_FOUND);
    }
}


static void ap_handler(void* main_data, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if(event_id == WIFI_EVENT_AP_STACONNECTED){
        device_set_state(BIT_IS_AP_CLIENT);
    } else if(event_id == WIFI_EVENT_AP_STADISCONNECTED){
        device_clear_state(BIT_IS_AP_CLIENT);
    }
}

int wifi_init(void) 
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_mode = WIFI_MODE_NULL;
    CHECK_AND_RET_ERR(esp_event_loop_create_default());
    CHECK_AND_RET_ERR(esp_netif_init());
    CHECK_AND_RET_ERR(esp_wifi_init(&cfg));
    CHECK_AND_RET_ERR(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    return ESP_OK;
}


int connect_sta(const char *ssid, const char *pwd)
{
    const size_t ssid_len = strnlen(ssid, sizeof(wifi_sta_config.sta.ssid));
    const size_t pwd_len = strnlen(pwd, sizeof(wifi_sta_config.sta.password));
    if( pwd_len < MIN_WIFI_PWD_LEN){
        ESP_LOGE("", "password is not configure");
        return ESP_ERR_WIFI_PASSWORD;
    }
    if(ssid_len == 0 || ssid_len == sizeof(wifi_sta_config.sta.ssid)){
        ESP_LOGE("", "ssid is not configure");
        return ESP_ERR_WIFI_SSID;
    }
    memset(&wifi_sta_config, 0, sizeof(wifi_sta_config));
    strncpy((char *)wifi_sta_config.sta.ssid, ssid, sizeof(wifi_sta_config.sta.ssid)-1);
    strncpy((char *)wifi_sta_config.sta.password, pwd, sizeof(wifi_sta_config.sta.password)-1);
    wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    if (wifi_mode == WIFI_MODE_AP){
        wifi_stop(); 
    }

    wifi_mode = WIFI_MODE_STA;
    
    if(netif == NULL){
        netif = esp_netif_create_default_wifi_sta();
        if(netif == NULL) return ESP_FAIL;
        CHECK_AND_RET_ERR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &sta_handler, NULL));
        CHECK_AND_RET_ERR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &sta_handler, NULL));
        CHECK_AND_RET_ERR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &sta_handler, NULL));
        CHECK_AND_RET_ERR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_STOP, &sta_handler, NULL));
    }
    
    CHECK_AND_RET_ERR(esp_wifi_set_mode(WIFI_MODE_STA));
#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
        CHECK_AND_RET_ERR(esp_wifi_set_protocol(ESP_IF_WIFI_STA,
                                                WIFI_PROTOCOL_11B
                                                |WIFI_PROTOCOL_11G
                                                |WIFI_PROTOCOL_11N
                                                |WIFI_PROTOCOL_LR));
#endif

    CHECK_AND_RET_ERR(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config));
    CHECK_AND_RET_ERR(esp_wifi_start());
    device_clear_state(BIT_IS_STA_CONNECTION);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    unsigned bits = device_wait_bits(BIT_IS_STA_CONNECTION);
    if(bits&BIT_IS_STA_CONNECTION){
        vTaskDelay(1000/portTICK_PERIOD_MS);
        return ESP_OK;
    }
    ESP_LOGE("", "err timeout sta");
    return ESP_ERR_TIMEOUT;
}


int start_ap()
{
    if (wifi_mode != WIFI_MODE_AP && wifi_mode != WIFI_MODE_NULL) {
        wifi_stop(); 
    }

    if(netif == NULL){
        netif = esp_netif_create_default_wifi_ap();
        if(netif == NULL) return ESP_FAIL;
        CHECK_AND_RET_ERR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &ap_handler, NULL));
        CHECK_AND_RET_ERR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &ap_handler, NULL));       
        CHECK_AND_RET_ERR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &ap_handler, NULL));
    }

    wifi_mode = WIFI_MODE_AP;
    CHECK_AND_RET_ERR(esp_wifi_set_mode(WIFI_MODE_AP));
    CHECK_AND_RET_ERR(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config));

    int res = esp_wifi_start();
    if(res == ESP_OK){
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
    return res;
}


void wifi_stop()
{
    device_clear_state(BIT_IS_AP_CLIENT|BIT_IS_STA_CONNECTION);
    if (wifi_mode == WIFI_MODE_AP){
        esp_wifi_stop();
        vTaskDelay(100/portTICK_PERIOD_MS);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &ap_handler);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &ap_handler);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_START, &ap_handler);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_STOP, &ap_handler);
    } else if (wifi_mode == WIFI_MODE_STA) {
        esp_wifi_stop();
        vTaskDelay(100/portTICK_PERIOD_MS);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &sta_handler);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &sta_handler);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &sta_handler);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_STOP, &sta_handler);
    }
    wifi_mode = WIFI_MODE_NULL;
    if(netif){
        esp_netif_destroy_default_wifi(netif);
        netif = NULL;
    }
}

