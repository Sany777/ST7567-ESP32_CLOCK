#pragma once
#include <string.h>
#include <stdint.h>

#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DHT20_I2C_ADDRESS      0x38
#define DHT20_MEASURE_TIMEOUT   1000





uint8_t dht20_status(void);
int dht20_read_data(float *temperature, float *humidity);
bool dht20_is_calibrated(void);
int dht20_wait();



#ifdef __cplusplus
}
#endif
