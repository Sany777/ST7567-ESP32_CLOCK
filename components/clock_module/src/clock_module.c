#include "clock_module.h"

#include <time.h>
#include "string.h"
#include "esp_sntp.h"
#include "wifi_service.h"
#include "esp_err.h"

#include "device_common.h"
#include "periodic_task.h"
#include "toolbox.h"

#define INTERVAL_8_HOUR   (1000*60*60*8)



int get_time_sec(struct tm* tinfo)
{
    return tinfo->tm_hour*3600 + tinfo->tm_min*60 + tinfo->tm_sec;
}

struct tm* get_time_tm(void)
{
    time_t time_now;
    time(&time_now);
    return localtime(&time_now);
}


void set_time_ms(long long time_ms)
{
    int offet_sec = device_get_offset() * 60 * 60;
    struct timeval tv;
    tv.tv_sec = time_ms/1000 + offet_sec;
    tv.tv_usec = time_ms%1000;
    settimeofday(&tv, NULL);
    device_set_state(BIT_IS_TIME);
}

void set_offset(int offset_hour)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tv.tv_sec = tv.tv_sec + offset_hour*60*60;
    settimeofday(&tv, NULL);
}


static void set_time_cb(struct timeval *tv)
{
    tv->tv_sec += 3600 * device_get_offset();
    settimeofday(tv, NULL);
    device_set_state(BIT_NEW_MIN|BIT_IS_TIME|BIT_SNTP_OK);
}


void init_sntp()
{
    if(esp_sntp_enabled()){
        esp_sntp_restart();
    } else {
        esp_sntp_set_time_sync_notification_cb(set_time_cb);
        esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.windows.com");
        sntp_servermode_dhcp(0);
        esp_sntp_set_sync_interval(INTERVAL_8_HOUR);
        esp_sntp_init();
    }
}


void stop_sntp()
{
    esp_sntp_stop();
    device_clear_state(BIT_SNTP_OK);
}

// format :
// %a: Аббревіатура дня тижня (Mon, Tue, ...)
// %A: Повна назва дня тижня (Monday, Tuesday, ...)
// %b: Аббревіатура місяця (Jan, Feb, ...)
// %B: Повна назва місяця (January, February, ...)
// %c: Дата і час (Sat Aug 23 14:55:02 2023)
// %d: День місяця (01 до 31)
// %H: Години в 24-годинному форматі (00 до 23)
// %I: Години в 12-годинному форматі (01 до 12)
// %j: День року (001 до 366)
// %m: Місяць (01 до 12)
// %M: Хвилини (00 до 59)
// %p: AM або PM
// %S: Секунди (00 до 60)
// %U: Номер тижня в році (неділя перший день тижня)
// %w: День тижня (неділя = 0, понеділок = 1, ...)
// %W: Номер тижня в році (понеділок перший день тижня)
// %x: Дата (08/23/23)
// %X: Час (14:55:02)
// %y: Останні дві цифри року (00 до 99)
// %Y: Повний рік (2023)
// %Z: Часовий пояс (UTC, GMT, ...)
const char* snprintf_time(const char *format)
{
    static char text_buf[100];
    struct tm *timeinfo = get_time_tm();
    strftime(text_buf, sizeof(text_buf), format, timeinfo);
    return text_buf;
}

