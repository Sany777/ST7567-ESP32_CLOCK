#ifndef CLOCK_MODULE_H
#define CLOCK_MODULE_H


#ifdef __cplusplus
extern "C" {
#endif


#include <sys/time.h>


struct tm* get_cur_time_tm(void);
void init_sntp();
void stop_sntp();
const char* snprintf_time(const char *format);
void set_time_ms(long long time_ms);
void set_offset(int offset_hour);




#ifdef __cplusplus
}
#endif





















#endif