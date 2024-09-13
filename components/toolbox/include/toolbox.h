#ifndef ADD_FUNCTIONS_H
#define ADD_FUNCTIONS_H



#ifdef __cplusplus
extern "C" {
#endif

#include "time.h"

uint8_t battery_voltage_to_percentage(float voltage);
unsigned get_num(char *data, unsigned size);
char * num_to_str(char *buf, unsigned num, unsigned char digits, const unsigned char base);
unsigned num_arr_to_str(char *dst, unsigned *src, unsigned char dst_digits, unsigned src_size);
int get_actual_forecast_data_index(const int cur_hour, const int update_data_time);


#ifdef __cplusplus
}
#endif


#endif



