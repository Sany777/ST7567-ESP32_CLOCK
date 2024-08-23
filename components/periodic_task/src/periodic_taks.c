#include "periodic_task.h"

#include "esp_timer.h"
#include "esp_task.h"
#include "portmacro.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_TASKS_NUM 10

static uint64_t ms;

typedef struct {
    periodic_func_in_isr_t func;
    int delay_init;
    int count;
    int delay;
}periodic_task_data_t;

esp_timer_handle_t periodic_timer = NULL;
periodic_task_data_t periodic_task[MAX_TASKS_NUM] = {0};
portMUX_TYPE periodic_timers_s = portMUX_INITIALIZER_UNLOCKED;



static IRAM_ATTR void periodic_timer_cb(void* arg);
static int init_timer(void);


periodic_task_data_t* IRAM_ATTR find_task(periodic_func_in_isr_t func)
{
    if(!func)return NULL;
    periodic_task_data_t 
        *end = periodic_task+MAX_TASKS_NUM, 
        *ptr = periodic_task;
    while(ptr < end){
        if(ptr->func == func){
            return ptr;
        } 
        ++ptr;
    }
    return NULL;
}

void periodic_task_remove(periodic_func_in_isr_t func)
{
    periodic_task_data_t*to_delete = find_task(func);
    if(to_delete){
        to_delete->count = to_delete->delay = 0;
    }
}

int IRAM_ATTR periodic_task_isr_create(periodic_func_in_isr_t func,
                            unsigned delay_ms, 
                            unsigned count)
{
    int res = ESP_FAIL;
    task_runner_stop();
    periodic_task_data_t *end = periodic_task+MAX_TASKS_NUM, *ptr = periodic_task;
    periodic_task_data_t *to_insert = find_task(func);
    while(to_insert == NULL && ptr < end){
        if(ptr->delay == 0){
            to_insert = ptr;
            to_insert->func = func;
        } else {
            ++ptr;
        }
    }

    if(to_insert){
        to_insert->delay = delay_ms;
        to_insert->delay_init = delay_ms;
        to_insert->count = count;
        res = ESP_OK;
    }
    device_timer_start();
    return res;
}

void IRAM_ATTR start_timer()
{
    ms = 0;
}

long long IRAM_ATTR get_timer_ms()
{
    return ms;
}



static  void IRAM_ATTR periodic_timer_cb(void*)
{
    const periodic_task_data_t *end = periodic_task+MAX_TASKS_NUM;
    periodic_task_data_t *ptr = periodic_task;
    while(ptr < end){
        if(ptr->delay > 0){
            --ptr->delay;
            if(ptr->delay == 0){
                if(ptr->count > 0) --ptr->count;
                if(ptr->count != 0){
                    ptr->delay = ptr->delay_init;
                }
                ptr->func();
            }
        }
        ++ptr;
    }
    ++ms;
}

static int init_timer(void)
{
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &periodic_timer_cb,
        .arg = NULL,
        .name = "tasks timer",
        .skip_unhandled_events = false
    };
    return esp_timer_create(&periodic_timer_args, &periodic_timer);
}

void device_timer_start()
{
    if(periodic_timer == NULL){
        init_timer();
    }
    if(periodic_timer && !esp_timer_is_active(periodic_timer)){
        esp_timer_start_periodic(periodic_timer, 1000);
    }
}

void task_runner_stop()
{
    if(periodic_timer && esp_timer_is_active(periodic_timer)){
        esp_timer_stop(periodic_timer);
    }
}

void task_runner_deinit()
{
    if(periodic_timer){
        esp_timer_stop(periodic_timer);
        esp_timer_delete(periodic_timer);
        periodic_timer = NULL;
    }
}

