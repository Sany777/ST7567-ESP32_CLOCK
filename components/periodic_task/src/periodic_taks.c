#include "periodic_task.h"

#include "esp_timer.h"
#include "esp_task.h"
#include "portmacro.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "device_common.h"
#include "clock_module.h"
#include "device_macro.h"

#define MAX_TASKS_NUM 15


typedef struct {
    periodic_func_t func;
    int delay_init;
    int count;
    uint64_t delay;
}periodic_task_list_data_t;

esp_timer_handle_t periodic_timer = NULL;
SemaphoreHandle_t timer_semaphore;


static long long time_val = 0, before_sleep = 0;
static periodic_task_list_data_t periodic_task_list[MAX_TASKS_NUM] = {0};


static periodic_task_list_data_t* find_task(periodic_task_list_data_t *list, 
                                                        size_t list_size, 
                                                        periodic_func_t func);
static int insert_task_to_list(periodic_task_list_data_t *list, 
                                            size_t list_size, 
                                            periodic_func_t func,
                                            uint64_t delay_ms, 
                                            int count);

static  void periodic_timer_cb(void*);

static periodic_task_list_data_t* IRAM_ATTR find_task(periodic_task_list_data_t *list, 
                                                        size_t list_size, 
                                                        periodic_func_t func)
{
    if(!func || !list) return NULL;
    const periodic_task_list_data_t *end = list+list_size;
    while(list < end){
        if(list->func == func){
            return list;
        } 
        ++list;
    }
    return NULL;
}


void remove_task(periodic_func_t func)
{
    device_stop_timer();
    if (xSemaphoreTake(timer_semaphore, 10000/portTICK_PERIOD_MS) == pdTRUE) {
        periodic_task_list_data_t*to_delete = find_task(periodic_task_list, MAX_TASKS_NUM, func);
        if(to_delete){
            to_delete->count = to_delete->delay = 0;
        }
        xSemaphoreGive(timer_semaphore);
    }
    device_start_timer();
}

static int IRAM_ATTR insert_task_to_list(
                            periodic_task_list_data_t *list, 
                            size_t list_size, 
                            periodic_func_t func,
                            uint64_t delay, 
                            int count)
{
    int res = ESP_FAIL;
    periodic_task_list_data_t 
        *end = list+MAX_TASKS_NUM;
    periodic_task_list_data_t *to_insert = find_task(list, MAX_TASKS_NUM, func);
    if(to_insert == NULL){
        while(list < end){
            if(list->count == 0){
                to_insert = list;
                to_insert->func = func;
                break;
            }
            ++list;
        }
    }

    if(to_insert){
        to_insert->delay_init = to_insert->delay = delay;
        to_insert->count = count;
        res = ESP_OK;
    }
    return res;
}

int IRAM_ATTR create_periodic_task(periodic_func_t func,
                            uint64_t delay_ms, 
                            int count)
{
    int res = ESP_FAIL;
    device_stop_timer();
    if (xSemaphoreTake(timer_semaphore, portMAX_DELAY) == pdTRUE) {
        res = insert_task_to_list(periodic_task_list, 
                                        MAX_TASKS_NUM, 
                                        func, 
                                        delay_ms, 
                                        count);
        xSemaphoreGive(timer_semaphore);
    }
    device_start_timer();
    return res;
}



void device_stop_timer()
{
    before_sleep = esp_timer_get_time();
    if(esp_timer_is_active(periodic_timer)){
        esp_timer_stop(periodic_timer);
    }
}

int device_init_timer()
{
    timer_semaphore = xSemaphoreCreateMutex();
    if(timer_semaphore){
        const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_cb,
            .arg = NULL,
            .name = "device timer",
            .skip_unhandled_events = true
        };
        return esp_timer_create(&periodic_timer_args, &periodic_timer);
    }
    return ESP_FAIL;
}

int device_start_timer()
{
    int res = ESP_FAIL;
    if(!esp_timer_is_active(periodic_timer)){
        if(before_sleep != 0){
            time_val = esp_timer_get_time() - before_sleep + 1;
        } else {
            time_val = 1;
        }
        res = esp_timer_start_periodic(periodic_timer, 1000);
    }
    return res;
}

static  void IRAM_ATTR periodic_timer_cb(void*)
{
    periodic_task_list_data_t *list = periodic_task_list;
    const periodic_task_list_data_t *end = list+MAX_TASKS_NUM;
    while(list < end){
        if(list->delay > 0){
            list->delay -= MIN(time_val, list->delay);
            if(list->delay == 0){
                if(list->count > 0)list->count -= 1;
                if(list->count != 0)list->delay = list->delay_init;
                list->func();
            }
        }
        ++list;
    }
    time_val = 1;
}
