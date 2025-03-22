#include "toolbox.h"

#include "device_common.h"


#include "stdlib.h"

uint8_t battery_voltage_to_percentage(float voltage) 
{
    if (voltage > 3.9) return 100;
    if (voltage <= 3.3) return 0;
    return (uint8_t)((voltage - 3.3) * 100 / (3.9 - 3.3));
}

int get_actual_forecast_data_index(const int cur_hour, const int update_data_time)
{
    int time_dif;
    if(update_data_time < 0 
        || update_data_time > 23) return NO_DATA;

    if( cur_hour >= update_data_time){
        time_dif = cur_hour - update_data_time;
    } else {
        time_dif = 24 + cur_hour - update_data_time;
    }
    if(time_dif >= FORECAST_LIST_SIZE*3) return NO_DATA;
    return time_dif / 3;
}


unsigned get_num(char *data, const unsigned size)
{
	unsigned res = 0;
	const char *end = data+size;
	char c;
	while(data != end){
		c = *(data++);
		if(c<'0' || c>'9') break;
		res *= 10;
		res += c-'0';
	}
	return res;
}

char * num_to_str(char *buf, unsigned num, unsigned char digits, const unsigned char base)
{
    const char hex_chars[] = "0123456789ABCDEF";
    unsigned char d = digits;
    char tmp,
        * end = buf,
        * ptr = buf;
        
    do{
        *end = hex_chars[num % base];
        if(d < 2) break;
        d -= 1;
        num /= base;
        end += 1;
    }while(1);

    while (ptr < end) {
        tmp = *ptr;
        *ptr = *end;
        *end = tmp;
        ptr += 1;
        end -= 1;
    }
	buf[digits] = 0;
    return &buf[digits];
}


unsigned num_arr_to_str(char *dst, unsigned *src, unsigned char dst_digits, unsigned src_size)
{
    if(src == NULL || src_size == 0)return 0;
    unsigned *src_end = src+src_size;
    char *start_dst = dst;
    while(src < src_end){
        dst = num_to_str(dst, *src, dst_digits, 10);
        ++src;
    }
    return dst - start_dst;
}





