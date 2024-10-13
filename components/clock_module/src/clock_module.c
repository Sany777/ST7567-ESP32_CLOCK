#include "clock_module.h"

#include <sys/time.h>
#include "string.h"
#include "esp_sntp.h"
#include "esp_err.h"
#include "stdbool.h"

#include "wifi_service.h"
#include "device_common.h"
#include "periodic_task.h"
#include "toolbox.h"





struct tm* get_cur_time_tm(void)
{
    time_t time_now;
    time(&time_now);
    return localtime(&time_now);
}


void set_time_sec(long long time_sec)
{
    struct timeval tv;
    tv.tv_sec = time_sec;
    settimeofday(&tv, NULL);
    device_set_state(BIT_IS_TIME);
}

void set_offset(int offset_hour)
{
    char tz[20]; 
    snprintf(tz, sizeof(tz), "UTC%+d", -offset_hour);
    setenv("TZ", tz, 1);
    tzset();  
}


static void set_time_cb(struct timeval *tv)
{
    device_set_state(BIT_EVENT_NEW_MIN|BIT_IS_TIME);
}


void init_sntp()
{
    if(esp_sntp_enabled()){
        esp_sntp_restart();
    } else {
        esp_sntp_set_time_sync_notification_cb(set_time_cb);
        esp_sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "0.ua.pool.ntp.org");
        esp_sntp_setservername(1, "1.ua.pool.ntp.org");
        esp_sntp_setservername(2, "2.ua.pool.ntp.org");
        esp_sntp_setservername(3, "pool.ntp.org");
        sntp_servermode_dhcp(1);
        esp_sntp_init();
    }
}



void stop_sntp()
{
    esp_sntp_stop();
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
    struct tm *timeinfo = get_cur_time_tm();
    strftime(text_buf, sizeof(text_buf), format, timeinfo);
    return text_buf;
}

