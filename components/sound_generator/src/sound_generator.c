#include "sound_generator.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include <math.h>

#include "device_common.h"
#include "periodic_task.h"


#define LEDC_TIMER              LEDC_TIMER_2
#define LEDC_MODE               LEDC_HIGH_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUT_RES            LEDC_TIMER_13_BIT 
#define DEFAULT_DUTY            (50) //  50%
#define DEFAULT_FREQUENCY       (2000) // 2 кГц
#define DEFAULT_DELAY           100


volatile static unsigned _delay, _loud;

static ledc_timer_config_t ledc_timer = {
    .speed_mode       = LEDC_MODE,
    .timer_num        = LEDC_TIMER,
    .duty_resolution  = LEDC_DUT_RES,
    .freq_hz          = 0,
    .clk_cfg          = LEDC_AUTO_CLK
};
static ledc_channel_config_t ledc_channel = {
    .speed_mode     = LEDC_MODE,
    .channel        = LEDC_CHANNEL,
    .timer_sel      = LEDC_TIMER,
    .intr_type      = LEDC_INTR_DISABLE,
    .gpio_num       = PIN_SIG_OUT,
    .duty           = 0, 
    .hpoint         = 0
};

static void IRAM_ATTR stop_signale();

static void init_pwm(unsigned freq_hz);
static void start_pwm(unsigned duty);
static void alarm();

static void IRAM_ATTR continue_signale()
{
    device_set_state_isr(BIT_WAIT_SIGNALE);
    ledc_timer_resume(ledc_timer.speed_mode, ledc_timer.timer_num);
    create_periodic_task(stop_signale, _delay/2, 1);
}

void start_single_signale(unsigned delay, unsigned freq)
{
    start_signale_series(delay, 1, freq);
}

void set_loud(unsigned loud)
{
    _loud = loud;
}

static void alarm()
{
    start_signale_series(100, 5, 1200);
}

void start_alarm()
{
    create_periodic_task(alarm, 1000, 5);
}

void sound_off()
{
    remove_task(alarm);
    remove_task(continue_signale);
    remove_task(stop_signale);
    stop_signale();
}

void start_signale_series(unsigned delay, unsigned count, unsigned freq)
{
    if( !(device_get_state()&BIT_WAIT_SIGNALE)){
        init_pwm(freq);
        if(delay == 0)_delay = DEFAULT_DELAY;
        else _delay = delay*2;
        start_pwm(_loud);
        if(count>1){
            create_periodic_task(continue_signale, _delay, count-1);
        }
        create_periodic_task(stop_signale, _delay/2, 1);
    }
}

static IRAM_ATTR void stop_signale()
{
    ledc_timer_pause(ledc_timer.speed_mode, ledc_timer.timer_num);
    device_clear_state_isr(BIT_WAIT_SIGNALE);
}

static void init_pwm(unsigned freq_hz)
{
    ledc_timer.freq_hz = freq_hz;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

static void start_pwm(unsigned duty)
{
    device_set_state_isr(BIT_WAIT_SIGNALE);
    if(duty > 99 || duty == 0) duty = DEFAULT_DUTY;
    ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, duty);
    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
}
