#include "periodic_task.h"

#include "esp_timer.h"
#include "esp_task.h"
#include "portmacro.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "clock_module.h"
#include "device_macro.h"

#define MAX_TASKS_NUM 15

volatile static uint64_t ms;

typedef struct {
    periodic_func_t func;
    int delay_init;
    int count;
    uint64_t delay;
}periodic_task_list_data_t;

esp_timer_handle_t periodic_timer = NULL;
TaskHandle_t task_runner_handle;

periodic_task_list_data_t periodic_isr_task_list[MAX_TASKS_NUM] = {0};
periodic_task_list_data_t periodic_task_list[MAX_TASKS_NUM] = {0};
portMUX_TYPE periodic_timers_s = portMUX_INITIALIZER_UNLOCKED;


static void runner_task(void *pvParameters);
static periodic_task_list_data_t* find_task(periodic_task_list_data_t *list, 
                                                        size_t list_size, 
                                                        periodic_func_t func);
static int periodic_task_create(periodic_task_list_data_t *list, 
                                            size_t list_size, 
                                            periodic_func_t func,
                                            uint64_t delay_ms, 
                                            unsigned count);
static void tasks_run(periodic_task_list_data_t *list, size_t list_size, uint64_t time_val);
static  void periodic_timer_cb(void*);

static periodic_task_list_data_t* IRAM_ATTR find_task(periodic_task_list_data_t *list, 
                                                        size_t list_size, 
                                                        periodic_func_t func)
{
    if(!func)return NULL;
    periodic_task_list_data_t *end = list+list_size;
    while(list < end){
        if(list->func == func){
            return list;
        } 
        ++list;
    }
    return NULL;
}

void remove_isr_task(periodic_func_t func)
{
    periodic_task_list_data_t*to_delete = find_task(periodic_isr_task_list, MAX_TASKS_NUM, func);
    if(to_delete){
        to_delete->count = to_delete->delay = 0;
    }
}

void remove_task(periodic_func_t func)
{
    periodic_task_list_data_t*to_delete = find_task(periodic_task_list, MAX_TASKS_NUM, func);
    if(to_delete){
        to_delete->count = to_delete->delay = 0;
    }
}

static int IRAM_ATTR periodic_task_create(
                            periodic_task_list_data_t *list, 
                            size_t list_size, 
                            periodic_func_t func,
                            uint64_t delay, 
                            unsigned count)
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

int IRAM_ATTR create_periodic_isr_task(periodic_func_t func,
                            uint64_t delay_ms, 
                            unsigned count)
{
    if(periodic_timer && esp_timer_is_active(periodic_timer)){
        esp_timer_stop(periodic_timer);
    }
    int res = periodic_task_create(periodic_isr_task_list, 
                                    MAX_TASKS_NUM, 
                                    func, 
                                    delay_ms, 
                                    count);
    if(periodic_timer == NULL){
        const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_cb,
            .arg = NULL,
            .name = "tasks timer",
            .skip_unhandled_events = true
        };
        res = esp_timer_create(&periodic_timer_args, &periodic_timer);
    }
    if(periodic_timer && !esp_timer_is_active(periodic_timer)){
        res = esp_timer_start_periodic(periodic_timer, 1000);
    }
    return res;
}





int IRAM_ATTR create_periodic_task(periodic_func_t func,
                            uint64_t delay_ms, 
                            unsigned count)
{
    if(task_runner_handle){
        vTaskSuspend(task_runner_handle);
    }
    int res = periodic_task_create(periodic_task_list, 
                                    MAX_TASKS_NUM, 
                                    func, 
                                    delay_ms, 
                                    count);
    if(task_runner_handle){
        vTaskResume(task_runner_handle);
    } else {
        xTaskCreate(runner_task, "task_runner", 5000, NULL, 5, &task_runner_handle);
        if(task_runner_handle == NULL) return ESP_FAIL;
    }
    return res;
}

void IRAM_ATTR restart_timer()
{
    ms = 0;
}

long long IRAM_ATTR get_timer_ms()
{
    return ms;
}

static void tasks_run(periodic_task_list_data_t *list, size_t list_size, uint64_t time_val)
{
    const periodic_task_list_data_t *end = list+list_size;
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
}

static  void IRAM_ATTR periodic_timer_cb(void*)
{
    tasks_run(periodic_isr_task_list, MAX_TASKS_NUM, 1);
    ++ms;
}




void task_runner_deinit()
{
    if(periodic_timer){
        esp_timer_stop(periodic_timer);
        esp_timer_delete(periodic_timer);
        periodic_timer = NULL;
    }
    if(task_runner_handle){
        vTaskDelete(task_runner_handle);
        task_runner_handle = NULL;
    }
}

static void runner_task(void *pvParameters)
{
    unsigned time_dif_sec;
    struct tm *tinfo = get_time_tm();
    int cur_time = get_time_sec(tinfo);
    int last_time_val = cur_time;
    bool init = false;
    for(;;){
        vTaskDelay(1000/portTICK_PERIOD_MS);
        if(! init && tinfo->tm_year != 70){
            time_dif_sec = 1;
            init = true;
        } else {
            cur_time = get_time_sec(tinfo);
            if(cur_time > last_time_val){
                time_dif_sec = cur_time - last_time_val;
            } else {
                time_dif_sec = cur_time + 86400 - last_time_val;
            }
        }
        tasks_run(periodic_task_list, MAX_TASKS_NUM, time_dif_sec*1000);
        last_time_val = cur_time;
    }
}