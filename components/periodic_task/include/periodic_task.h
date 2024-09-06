#ifndef PERIODIC_TAKS_H
#define PERIODIC_TAKS_H


#ifdef __cplusplus
extern "C" {
#endif


#include "stdint.h"

#define FOREVER -1

typedef void(*periodic_func_t)();





void remove_isr_task(periodic_func_t func);
void remove_task(periodic_func_t func);
int create_periodic_isr_task(periodic_func_t func,
                            uint64_t delay_ms, 
                            unsigned count);
int create_periodic_task(periodic_func_t func,
                            uint64_t delay_ms, 
                            unsigned count);
void restart_timer();
long long get_timer_ms();
void task_runner_deinit();






#ifdef __cplusplus
}
#endif

#endif



