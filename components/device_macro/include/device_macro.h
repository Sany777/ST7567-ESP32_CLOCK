#ifndef DEVICE_MACRO_H
#define DEVICE_MACRO_H

#ifdef __cplusplus
extern "C" {
#endif


#include "esp_log.h"
#include "esp_err.h"
#include "stddef.h"


#define CHECK_AND_RET_ERR(result_) \
    do{ \
        const int e = result_; \
        if(e != ESP_OK){ ESP_LOGE(__func__, "%s", esp_err_to_name(e)); return e; } \
    }while(0)

#define CHECK_AND_GO(result_, label_) \
    do{ \
        const int e = result_; \
        if(e != ESP_OK){ ESP_LOGE(__func__, "%s", esp_err_to_name(e)); goto label_; } \
    }while(0)

#define CHECK_AND_RET(err_) \
    do{ \
        const int e = err_; \
        if(e != ESP_OK){ ESP_LOGE(__func__, "%s", esp_err_to_name(e)); return; } \
    }while(0)

#define flag_reset(flags, index)    \
    ((flags) &= ~(1<<(index)))

#define flag_set(flags, index)   \
    ((flags) |= (1<<(index)))

#define flag_set_value(flags, index, val)   \
    ((val)?flag_set((flags),(index)):flag_reset((flags),(index)))

#define flag_get(flags, index)  \
    ((flags)&(1<<(index)))

#define MIN(a,b)    \
    ((a)>(b)?(b):(a))



#ifdef __cplusplus
}
#endif



#endif 
