#include "forecast_http_client.h"

#include "clock_module.h"
#include "device_common.h"
#include "device_macro.h"


#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define OPENWEATHER_SERVER_HOST "api.openweathermap.org"
#define SERVER_PORT "80" 
#define SIZE_URL_BUF 256
#define FORECAST_LIST_SIZE 5
#define MAX_RETRIES 3  
#define RETRY_DELAY_MS 1000  

#define MAX_KEY_NUM 10

extern char network_buf[];

bool fetch_data(const char *request, char *response_buf, uint32_t *resp_size) ;
static size_t get_value_ptrs(char ***value_list, char *data_buf, const size_t buf_len, const char *key);
static void split(char *data_buf, const char *split_chars_str, uint32_t data_size);


bool fetch_weather_data(const char *city, const char *api_key, uint32_t *resp_size) 
{
    char request[SIZE_URL_BUF + 128];
    snprintf(request, sizeof(request),
        "GET /data/2.5/forecast?q=%s&units=metric&cnt=%d&appid=%s HTTP/1.1\r\n"
        "Host: " OPENWEATHER_SERVER_HOST "\r\n"
        "Connection: close\r\n\r\n",
        city, FORECAST_LIST_SIZE, api_key);
    return fetch_data(request, network_buf, resp_size);
}

#define TIME_SERVER_HOST "worldtimeapi.org"

bool fetch_time_data(uint32_t *resp_size) 
{
    return true;
    char request[SIZE_URL_BUF + 128];
    snprintf(request, sizeof(request),
        "GET /api/timezone/Etc/UTC HTTP/1.1\r\n"
        "Host: " TIME_SERVER_HOST "\r\n"
        "Connection: close\r\n\r\n");
    return fetch_data(request, network_buf, resp_size);
}

bool device_update_time()
{
    uint32_t data_size = 0;
    char ** dateTime = NULL;
    if(fetch_time_data(&data_size)){
        get_value_ptrs(&dateTime, network_buf, data_size, "\"dateTime\":");
        if(dateTime){
            struct tm tinfo;
            sscanf(dateTime[0],"%d-%d-%dT%d:%d:%d", 
                    &tinfo.tm_yday, 
                    &tinfo.tm_mon, 
                    &tinfo.tm_mday, 
                    &tinfo.tm_hour, 
                    &tinfo.tm_min, 
                    &tinfo.tm_sec);

            time_t epoch_time = mktime(&tinfo);

            struct timeval now = { .tv_sec = epoch_time };
            settimeofday(&now, NULL);
            free(dateTime);
            dateTime = NULL;
            return true;
        }
    }
    return false;
}



bool fetch_data(const char *request, char *response_buf, uint32_t *resp_size) 
{
    *resp_size = 0;
    struct addrinfo hints = {0}, *res;
    int sock = -1;
    int retries = 0;
    while (retries < MAX_RETRIES) {
        retries++;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(OPENWEATHER_SERVER_HOST, SERVER_PORT, &hints, &res) != 0) {
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            continue;
        }

        sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) {
            freeaddrinfo(res);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            continue;
        }

        if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
            close(sock);
            freeaddrinfo(res);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            continue;
        }
        freeaddrinfo(res); 

        if (send(sock, request, strlen(request), 0) < 0) {
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            continue;
        }

        *resp_size = recv(sock, response_buf, NET_BUF_LEN - 1, 0);
        if (*resp_size > 0) {
            response_buf[*resp_size] = '\0'; 
            close(sock);
            return true;  
        } else {
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        }
    }
    return false; 
}

bool update_forecast_data(const char *city, const char *api_key)
{
    uint32_t data_size = 0;
    char **feels_like_list = NULL, **description_list = NULL, **pop_list = NULL, **dt_list = NULL; 

    if(strnlen(city, MAX_STR_LEN) == 0 || strnlen(api_key, MAX_STR_LEN) != API_LEN)
        return false;
    if(fetch_weather_data(city, api_key, &data_size)){
        const size_t pop_num = get_value_ptrs(&pop_list, network_buf, data_size, "\"pop\":");
        const size_t feels_like_num = get_value_ptrs(&feels_like_list, network_buf, data_size, "\"feels_like\":");
        const size_t description_num = get_value_ptrs(&description_list, network_buf, data_size, "\"description\":\"");
        get_value_ptrs(&dt_list, network_buf, data_size,"\"dt\":");
        split(network_buf, "},\"", data_size);
        if(description_list){
            memset(service_data.desciption, 0, sizeof(service_data.desciption));
            for(int i=0; i<description_num && i<FORECAST_LIST_SIZE; ++i){
                strncpy(service_data.desciption[i], description_list[i], sizeof(service_data.desciption[0]));
            }
            free(description_list);
            description_list = NULL;
        }

        if(dt_list){
            time_t time_now  = atol(dt_list[0]);
            struct tm * tinfo = localtime(&time_now);
            service_data.update_data_time = tinfo->tm_hour;
            free(dt_list);
            dt_list = NULL;
        }

        if(feels_like_list){
            for(int i=0; i<feels_like_num && i<FORECAST_LIST_SIZE; ++i){
                service_data.temp_list[i] = atof(feels_like_list[i]);
            }
            free(feels_like_list);
            feels_like_list = NULL;
        }

        if(pop_list){
            for(int i=0; i<pop_num && i<FORECAST_LIST_SIZE; ++i){
                service_data.pop_list[i] = atof(pop_list[i])*100;
            }
            free(pop_list);
            pop_list = NULL;
        }
        return true;
    }
    return false;
}



static size_t get_value_ptrs(char ***value_list, char *data_buf, const size_t buf_len, const char *key)
{
    char *buf_oper[MAX_KEY_NUM] = { 0 };
    if(data_buf == NULL) 
        return 0;
    const size_t key_size = strlen(key);
    char *data_ptr = data_buf;
    char **buf_ptr = buf_oper;
    const char *data_end = data_buf+buf_len;
    size_t value_list_size = 0;
    while(data_ptr = strstr(data_ptr, key), 
            data_ptr 
            && data_end > data_ptr 
            && MAX_KEY_NUM > value_list_size ){
        data_ptr += key_size;
        buf_ptr[value_list_size++] = data_ptr;
    }
    const size_t data_size = value_list_size * sizeof(char *);
    *value_list = (char **)malloc(data_size);
    if(value_list == NULL)
        return 0;
    memcpy(*value_list, buf_oper, data_size);
    return value_list_size;
}


static void split(char *data_buf, const char *split_chars_str, uint32_t data_size)
{
    char *ptr = data_buf;
    const char *end_data_buf = data_buf + data_size;
    const size_t symb_list_size = strlen(split_chars_str);
    char c;
    while(ptr != end_data_buf){
        c = *(ptr);
        for(int i=0; i<symb_list_size; ++i){
            if(split_chars_str[i] == c){
                *(ptr) = 0;
                break;
            }
        }
        ++ptr;
    }
}