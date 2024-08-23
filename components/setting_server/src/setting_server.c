#include "setting_server.h"

#include <sys/stat.h>
#include <dirent.h>
#include "cJSON.h"
#include "stdbool.h"

#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "string.h"
#include "portmacro.h"
#include "clock_module.h"
#include "additional_functions.h"
#include "string.h"
#include "device_macro.h"
#include "wifi_service.h"
#include "device_common.h"


static httpd_handle_t server;

const char *MES_DATA_NOT_READ = "Data not read";
const char *MES_DATA_TOO_LONG = "Data too long";
const char *MES_NO_MEMORY = "No memory";
const char *MES_BAD_DATA_FOMAT = "wrong data format";
const char *MES_SUCCESSFUL = "Successful";



#define SEND_REQ_ERR(_req_, _str_, _fail_) \
    do{ httpd_resp_send_err((_req_), HTTPD_400_BAD_REQUEST, (_str_)); goto _fail_;}while(0)

#define SEND_SERVER_ERR(_req_, _str_, _fail_) \
    do{ httpd_resp_send_err((_req_), HTTPD_500_INTERNAL_SERVER_ERROR, (_str_)); goto _fail_;}while(0)


void server_stop()
{
    device_clear_state(BIT_SERVER_RUN);
}

static esp_err_t index_redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/index.html");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t get_index_handler(httpd_req_t *req)
{
    extern const unsigned char index_html_start[] asm( "_binary_index_html_start" );
    extern const unsigned char index_html_end[] asm( "_binary_index_html_end" );
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start); 
    return ESP_OK;
}

static esp_err_t get_css_handler(httpd_req_t *req)
{
    extern const unsigned char style_css_start[] asm( "_binary_style_css_start" );
    extern const unsigned char style_css_end[] asm( "_binary_style_css_end" );
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)style_css_start, style_css_end - style_css_start ); 
    return ESP_OK;
}

static esp_err_t get_js_handler(httpd_req_t *req)
{
    extern const unsigned char script_js_start[] asm( "_binary_script_js_start" );
    extern const unsigned char script_js_end[] asm( "_binary_script_js_end" );
    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_send(req, (const char *)script_js_start, script_js_end - script_js_start ); 
    return ESP_OK;
}



static esp_err_t handler_close(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "Goodbay!");
    vTaskDelay(500/portTICK_PERIOD_MS);
    device_clear_state(BIT_SERVER_RUN);
    return ESP_OK;
}



static esp_err_t handler_set_network(httpd_req_t *req)
{
    cJSON *root, *ssid_name_j, *pwd_wifi_j;
    int received;
    const size_t total_len = req->content_len;
    char * server_buf = (char *)req->user_ctx;
    if(total_len >= NET_BUF_LEN){
        SEND_REQ_ERR(req, MES_DATA_TOO_LONG, fail_1);
    }
    received = httpd_req_recv(req, server_buf, total_len);
    if (received != total_len) {
        SEND_SERVER_ERR(req, MES_DATA_NOT_READ, fail_1);
    }
    server_buf[received] = 0;
    root = cJSON_Parse(server_buf);
    if(!root){
        SEND_SERVER_ERR(req, MES_NO_MEMORY, fail_1);
    }

    ssid_name_j = cJSON_GetObjectItemCaseSensitive(root, "SSID");
    pwd_wifi_j = cJSON_GetObjectItemCaseSensitive(root, "PWD");
    
    if(cJSON_IsString(ssid_name_j) && (ssid_name_j->valuestring != NULL)){
        device_set_ssid(ssid_name_j->valuestring);
    }
    if(cJSON_IsString(pwd_wifi_j) && (pwd_wifi_j->valuestring != NULL)){
        device_set_pwd(pwd_wifi_j->valuestring);
    }
    cJSON_Delete(root);
    httpd_resp_sendstr(req, MES_SUCCESSFUL);
    return ESP_OK;

fail_1:
    return ESP_FAIL;
}

static esp_err_t handler_set_openweather_data(httpd_req_t *req)
{
    cJSON *root, *city_j, *key_j;
    int received;
    const int total_len = req->content_len;
    char * server_buf = (char *)req->user_ctx;
    if(total_len >= NET_BUF_LEN){
        SEND_REQ_ERR(req, MES_DATA_TOO_LONG, fail_1);
    }
    received = httpd_req_recv(req, server_buf, total_len);
    if (received != total_len) {
        SEND_SERVER_ERR(req, MES_DATA_NOT_READ, fail_1);
    }
    server_buf[received] = 0;
    root = cJSON_Parse(server_buf);
    if(!root){
        SEND_SERVER_ERR(req, MES_NO_MEMORY, fail_1);
    }
    city_j = cJSON_GetObjectItemCaseSensitive(root, "City");
    key_j = cJSON_GetObjectItemCaseSensitive(root, "Key");
    if(cJSON_IsString(city_j) && (city_j->valuestring != NULL)){
        device_set_city(city_j->valuestring);
    }
    if(cJSON_IsString(key_j) && (key_j->valuestring != NULL)){
        device_set_key(key_j->valuestring);
    }
    cJSON_Delete(root);
    httpd_resp_sendstr(req, MES_SUCCESSFUL);
    return ESP_OK;

fail_1:
    return ESP_FAIL;
}

const char *get_chip(int model_id)
{
    switch(model_id){
        case 1: return "ESP32";
        case 2: return "ESP32-S2";
        case 3: return "ESP32-S3";
        case 5: return "ESP32-C3";
        case 6: return "ESP32-H2";
        case 12: return "ESP32-C2";
        default: break;
    }
    return "uknown";
}

static esp_err_t handler_get_info(httpd_req_t *req)
{
    char * server_buf = (char *)req->user_ctx;
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    snprintf(server_buf, 200, "idf version %s\nchip %s\nrevision %u", 
                    IDF_VER, 
                    get_chip(chip_info.model), 
                    chip_info.revision);
    httpd_resp_sendstr(req, server_buf);
    return ESP_OK;
}

static esp_err_t handler_set_time(httpd_req_t *req)
{
    long long time;
    int received;
    const int total_len = req->content_len;
    char * server_buf = (char *)req->user_ctx;
    if(total_len >= NET_BUF_LEN){
        SEND_REQ_ERR(req, MES_DATA_TOO_LONG, fail);
    }
    received = httpd_req_recv(req, server_buf, total_len);
    if (received != total_len){
        SEND_REQ_ERR(req, MES_DATA_NOT_READ, fail);
    }
    server_buf[total_len] = 0;
    time = atoll(server_buf);
    set_time_ms(time);
    httpd_resp_sendstr(req, MES_SUCCESSFUL);
    return ESP_OK;
fail:
    return ESP_FAIL;
}




static esp_err_t handler_give_data(httpd_req_t *req)
{
    char *data_to_send;
    cJSON *root;
    char *notif_data_str = (char *)req->user_ctx;
    char schema_data_str[WEEK_DAYS_NUM*2+1];
    unsigned *schema = device_get_schema();
    unsigned *notify = device_get_notif();
    size_t notif_data_str_size = get_notif_size(schema);
    if(notif_data_str_size < NET_BUF_LEN){
        num_arr_to_str(notif_data_str,  notify, 4, notif_data_str_size);
    } else {
        notif_data_str_size = 0;
    }
    notif_data_str[notif_data_str_size] = 0;
    num_arr_to_str(schema_data_str, schema, 2, WEEK_DAYS_NUM);
    httpd_resp_set_type(req, "application/json");

    root = cJSON_CreateObject();
    if(!root){
        SEND_REQ_ERR(req, MES_NO_MEMORY, fail_1);
    }
    // input id | value
    cJSON_AddStringToObject(root, "SSID", device_get_ssid());
    cJSON_AddStringToObject(root, "PWD", device_get_pwd());
    cJSON_AddStringToObject(root, "Key", device_get_api_key());
    cJSON_AddStringToObject(root, "City", device_get_city_name());
    cJSON_AddNumberToObject(root, "Status", device_get_state());
    cJSON_AddStringToObject(root, "schema", schema_data_str);
    cJSON_AddStringToObject(root, "notif", notif_data_str);
    cJSON_AddNumberToObject(root, "Hour", device_get_offset());
    cJSON_AddNumberToObject(root, "%", device_get_loud());
    data_to_send = cJSON_Print(root);

    if(!data_to_send){
        SEND_REQ_ERR(req, MES_NO_MEMORY, fail_2);
    }
    httpd_resp_sendstr(req, data_to_send);
    free(data_to_send);
    data_to_send = NULL;
    cJSON_Delete(root);

    return ESP_OK;

fail_2:
    cJSON_Delete(root);

fail_1:

    return ESP_FAIL;
}

	

static esp_err_t handler_set_flag(httpd_req_t *req)
{
    unsigned flags;
    int received,
        total_len = req->content_len;
    char * server_buf = (char *)req->user_ctx;
    if(total_len >= NET_BUF_LEN){
        SEND_REQ_ERR(req, "Content too long", fail);
    }
    received = httpd_req_recv(req, server_buf, total_len);
    if (received != total_len) {
        SEND_REQ_ERR(req, "Data not read", fail);
    }
    server_buf[total_len] = 0;
    flags = atol(server_buf) & STORED_FLAGS;
    device_set_state(flags);
    httpd_resp_sendstr(req, "Set flags successfully");
    return ESP_OK;
fail:
    return ESP_FAIL;
}


static esp_err_t set_offset_handler(httpd_req_t *req)
{
    int received;
    const int total_len = req->content_len;
    char * server_buf = (char *)req->user_ctx;
    if(total_len >= NET_BUF_LEN){
        SEND_REQ_ERR(req, MES_DATA_TOO_LONG, fail_1);
    }
    received = httpd_req_recv(req, server_buf, total_len);
    if (received != total_len) {
        SEND_SERVER_ERR(req, MES_DATA_NOT_READ, fail_1);
    }
    server_buf[received] = 0;
    httpd_resp_sendstr(req, MES_SUCCESSFUL);
    int offset = atoi(server_buf);
    if(offset > 23 || offset < -23){
        SEND_REQ_ERR(req, MES_BAD_DATA_FOMAT, fail_1);
    }
    device_set_offset(offset);
    return ESP_OK;
    
fail_1:
    return ESP_FAIL;
}

static esp_err_t set_loud_handler(httpd_req_t *req)
{
    unsigned loud = 0;
    int received;
    const int total_len = req->content_len;
    char * server_buf = (char *)req->user_ctx;
    if(total_len >= NET_BUF_LEN){
        SEND_REQ_ERR(req, MES_DATA_TOO_LONG, fail_1);
    }
    received = httpd_req_recv(req, server_buf, total_len);
    if (received != total_len) {
        SEND_SERVER_ERR(req, MES_DATA_NOT_READ, fail_1);
    }
    server_buf[received] = 0;
    httpd_resp_sendstr(req, MES_SUCCESSFUL);
    loud = atol(server_buf);
    if(loud > 99){
        SEND_REQ_ERR(req, MES_BAD_DATA_FOMAT, fail_1);
    }
    device_set_loud(loud);
    return ESP_OK;
    
fail_1:
    return ESP_FAIL;
}

static esp_err_t set_notif_handler(httpd_req_t *req)
{
    unsigned schema_data[WEEK_DAYS_NUM],
        *notif_data, *ptr;
    size_t notif_size, schema_size;
    cJSON *root, *notif_data_j, *notif_schema_j;
    char *notif_str, *schema_str;
    int received, notification_num;
    const int total_len = req->content_len;
    char * server_buf = (char *)req->user_ctx;
    if(total_len >= NET_BUF_LEN){
        SEND_REQ_ERR(req, MES_DATA_TOO_LONG, fail_1);
    }
    received = httpd_req_recv(req, server_buf, total_len);
    if (received != total_len) {
        SEND_SERVER_ERR(req, MES_DATA_NOT_READ, fail_1);
    }
    server_buf[received] = 0;
    root = cJSON_Parse(server_buf);
    if(!root){
        SEND_REQ_ERR(req, MES_NO_MEMORY, fail_1);
    }

    notif_data_j = cJSON_GetObjectItemCaseSensitive(root, "notif");
    notif_schema_j = cJSON_GetObjectItemCaseSensitive(root, "schema");

    if(!cJSON_IsString(notif_data_j) || notif_data_j->valuestring == NULL
            || !cJSON_IsString(notif_schema_j) || notif_schema_j->valuestring == NULL){
        SEND_REQ_ERR(req, MES_BAD_DATA_FOMAT, fail_2);
    }
    schema_str = notif_schema_j->valuestring;
    notif_str = notif_data_j->valuestring;

    notif_size = strlen(notif_str);
    schema_size = strlen(schema_str);
    if(schema_size != 14 || notif_size%4 != 0 ){
        SEND_REQ_ERR(req, MES_BAD_DATA_FOMAT, fail_2);
    }

    for(int i=0; i<WEEK_DAYS_NUM; ++i, schema_str += 2){
        schema_data[i] = get_num(schema_str, 2);
    }
    notification_num = get_notif_num(schema_data);
    if(notification_num != notif_size/4){
        SEND_REQ_ERR(req, MES_BAD_DATA_FOMAT, fail_2);
    }
    notif_data = ptr = (unsigned *)malloc(notification_num*sizeof(int));
    if (notif_data == NULL) {
        SEND_SERVER_ERR(req, MES_DATA_NOT_READ, fail_2);
    }
    for(int i=0; i<notification_num; ++i, notif_str += 4){
        notif_data[i] = get_num(notif_str, 4);
    }
    device_set_notify_data(schema_data, notif_data);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, MES_SUCCESSFUL);
    return ESP_OK;

fail_2:
    cJSON_Delete(root);
fail_1:
    return ESP_FAIL;
}





int deinit_server()
{
    esp_err_t err = ESP_FAIL;
    if(server != NULL){
        err = httpd_stop(server);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        server = NULL;
    }
    return err;
}


int init_server(char *server_buf)
{
    if(server != NULL) return ESP_FAIL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 14;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if(httpd_start(&server, &config) != ESP_OK){
        return ESP_FAIL;
    }
    
    httpd_uri_t set_flags = {
        .uri      = "/Status",
        .method   = HTTP_POST,
        .handler  = handler_set_flag,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &set_flags);

    httpd_uri_t get_info = {
        .uri      = "/info?",
        .method   = HTTP_POST,
        .handler  = handler_get_info,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &get_info);

    httpd_uri_t get_setting = {
        .uri      = "/data?",
        .method   = HTTP_POST,
        .handler  = handler_give_data,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &get_setting);
    
     httpd_uri_t close_uri = {
        .uri      = "/close",
        .method   = HTTP_POST,
        .handler  = handler_close,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &close_uri);

     httpd_uri_t net_uri = {
        .uri      = "/Network",
        .method   = HTTP_POST,
        .handler  = handler_set_network,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &net_uri);

    httpd_uri_t api_uri = {
        .uri      = "/Openweather",
        .method   = HTTP_POST,
        .handler  = handler_set_openweather_data,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &api_uri);

     httpd_uri_t time_uri = {
        .uri      = "/time",
        .method   = HTTP_POST,
        .handler  = handler_set_time,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &time_uri);

     httpd_uri_t index_uri = {
        .uri      = "/index.html",
        .method   = HTTP_GET,
        .handler  = get_index_handler ,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &index_uri);

     httpd_uri_t get_style = {
        .uri      = "/style.css",
        .method   = HTTP_GET,
        .handler  = get_css_handler,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &get_style);

     httpd_uri_t get_script = {
        .uri      = "/script.js",
        .method   = HTTP_GET,
        .handler  = get_js_handler,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &get_script);

     httpd_uri_t notif_uri = {
        .uri      = "/Notification",
        .method   = HTTP_POST,
        .handler  = set_notif_handler,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &notif_uri);

     httpd_uri_t redir_uri = {
        .uri      = "/*",
        .method   = HTTP_GET,
        .handler  = index_redirect_handler,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &redir_uri);

    httpd_uri_t set_offset_uri = {
        .uri      = "/Offset",
        .method   = HTTP_POST,
        .handler  = set_offset_handler,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &set_offset_uri);

    httpd_uri_t set_loud_uri = {
        .uri      = "/Loud",
        .method   = HTTP_POST,
        .handler  = set_loud_handler,
        .user_ctx = server_buf
    };
    httpd_register_uri_handler(server, &set_loud_uri);

    vTaskDelay(100/portTICK_PERIOD_MS);
    return ESP_OK;
}


