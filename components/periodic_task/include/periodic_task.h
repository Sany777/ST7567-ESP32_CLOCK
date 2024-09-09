#ifndef PERIODIC_TAKS_H
#define PERIODIC_TAKS_H


#ifdef __cplusplus
extern "C" {
#endif


#include "stdint.h"

#define FOREVER -1

typedef void(*periodic_func_t)();





void remove_task(periodic_func_t func);
int create_periodic_task(periodic_func_t func,
                            uint64_t delay_ms, 
                            int count);
void device_stop_timer();
int device_start_timer();
int device_init_timer();






#ifdef __cplusplus
}
#endif

#endif



