#include "screen_handler.h"

#include "periodic_task.h"
#include "device_common.h"
#include "forecast_http_client.h"
#include "sound_generator.h"
#include "dht20.h"
#include "adc_reader.h"

#include "esp_sleep.h"
#include "wifi_service.h"
#include "stdbool.h"
#include "lcd.h"
#include "sdkconfig.h"
#include "clock_module.h"
#include "setting_server.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "portmacro.h"


#include "esp_log.h"



enum FuncId{
    SCREEN_MAIN,
    SCREEN_FORECAST_DETAIL,
    SCREEN_TIMER,
    SCREEN_SETTING,
    SCREEN_DEVICE_INFO,
};

enum TimeoutConst{
    TIMEOUT_SEC   = 1000,
    TIMEOUT_2_SEC = 2*TIMEOUT_SEC,
    ONE_MINUTE    = 60*TIMEOUT_SEC,
    TIMEOUT_4_HOUR= 4*60*ONE_MINUTE,
    TIMEOUT_5_SEC = 5*TIMEOUT_SEC,
    TIMEOUT_7_SEC = 7*TIMEOUT_SEC,
    TIMEOUT_6_SEC = 6*TIMEOUT_SEC,
    TIMEOUT_20_SEC= 20*TIMEOUT_SEC,
    HALF_MINUTE   = 30*TIMEOUT_SEC,
};



static void timer_func(int cmd);
static void setting_func(int cmd);
static void main_func(int cmd);
static void device_info_func(int cmd);
static void weather_info_func(int cmd);


typedef void(*handler_func_t)(int);

static const handler_func_t func_list[] = {
    main_func,
    weather_info_func,
    timer_func,
    setting_func,
    device_info_func,
};

static const int SCREEN_LIST_SIZE = sizeof(func_list)/sizeof(func_list[0]);

enum {
    NO_DATA = -1,
    CMD_PRESS,
    CMD_INC,
    CMD_DEC,
    CMD_INIT,
    CMD_DEINIT,
    CMD_UPDATE_DATA,
    CMD_UPDATE_TIME,
    CMD_UPDATE_ROT_VAL,
    CMD_UPDATE_TIMER_TIME
};

volatile static int next_screen;

static bool screen_inp;

void backlight_off()
{
    device_set_pin(PIN_LCD_BACKLIGHT_EN, 0);
    screen_inp = false;
    remove_task(backlight_off);
}

void backlight_on()
{
    device_set_pin(PIN_LCD_BACKLIGHT_EN, 1);
    screen_inp = true;
    create_periodic_task(backlight_off, 45, 1);
}


static void main_task(void *pv)
{
    unsigned bits;
    int cmd = NO_DATA;

    int screen = -1;
    uint64_t sleep_time_ms;
    bool wait_task;
    int timeout = TIMEOUT_6_SEC;
    device_set_state(BIT_UPDATE_FORECAST_DATA);
    vTaskDelay(100/portTICK_PERIOD_MS);
    for(;;){
        wait_task = true;
        next_screen = SCREEN_MAIN;
        restart_timer();
        do{
            
            cmd = NO_DATA;

            bits = device_get_state();

            if(bits&BIT_BUT_LONG_PRESSED){
                if(screen_inp){
                    backlight_off();
                } else {
                    backlight_on();
                }
                device_clear_state(BIT_BUT_LONG_PRESSED);
            }

            if(bits&BIT_ENCODER_ROTATE){
                start_single_signale(10, 2000);
                if(screen_inp){
                    next_screen += get_encoder_val();
                    reset_encoder_val();
                } else {
                    cmd = CMD_UPDATE_ROT_VAL;
                }
                device_clear_state(BIT_ENCODER_ROTATE);
            }

            if(screen != next_screen) {
                if(next_screen >= SCREEN_LIST_SIZE){
                    next_screen = 0;
                } else if(next_screen < 0){
                    next_screen = SCREEN_LIST_SIZE-1;
                }
                screen = next_screen;
                cmd = CMD_INIT;
            } else if(bits & BIT_NEW_DATA) {
                cmd = CMD_UPDATE_DATA;
                device_clear_state(BIT_NEW_DATA);
            } else if(bits&BIT_NEW_MIN) {
                device_clear_state(BIT_NEW_MIN);
                cmd = CMD_UPDATE_TIME; 
                if(bits & BIT_IS_TIME && !(bits & BIT_NOTIF_DISABLE) ){
                    if(is_signale(get_time_tm())){
                        start_signale_series(75, 7, 2000);
                    }
                }
            } else if( ! screen_inp
                        && ! (bits&BIT_WAIT_SIGNALE)
                            && ! (bits&BIT_WAIT_BUT_INPUT)
                                && ! (bits&BIT_WAIT_PROCCESS)
                                    && get_timer_ms() > timeout){
                wait_task = false;
                cmd = CMD_UPDATE_DATA;
            }
            
            if(cmd != NO_DATA){
                lcd_fill(UNCOLORED);
                func_list[screen](cmd);
                lcd_update();
            }

            vTaskDelay(250/portTICK_PERIOD_MS);

        } while(wait_task);

        wifi_stop();
        sleep_time_ms = ONE_MINUTE - get_timer_ms()%ONE_MINUTE;
        device_set_pin(PIN_DHT20_EN, 0);
        esp_sleep_enable_timer_wakeup(sleep_time_ms * 1000);
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_WAKEUP, 1);
        esp_light_sleep_start();
        device_set_pin(PIN_DHT20_EN, 1);    
    }
}


static void update_forecast_handler()
{
    device_set_state(BIT_UPDATE_FORECAST_DATA);
}


static void service_task(void *pv)
{
    uint32_t bits;
    int timeout = 0;
    bool open_sesion = false;
    vTaskDelay(100/portTICK_PERIOD_MS);
    int fail_count = 0;
    for(;;){
        device_wait_bits_untile(BIT_UPDATE_FORECAST_DATA
                            | BIT_INIT_SNTP
                            | BIT_START_SERVER, 
                            portMAX_DELAY);

        bits = device_set_state(BIT_WAIT_PROCCESS);
        if(bits & BIT_START_SERVER){
            if(start_ap() == ESP_OK ){
                bits = device_wait_bits(BIT_IS_AP_CONNECTION);
                if(bits & BIT_IS_AP_CONNECTION && init_server(network_buf) == ESP_OK){
                    device_set_state(BIT_NEW_DATA|BIT_SERVER_RUN);
                    open_sesion = false;
                    while(bits = device_get_state(), bits&BIT_SERVER_RUN){
                        if(open_sesion){
                            if(!(bits&BIT_IS_AP_CLIENT) ){
                                device_clear_state(BIT_SERVER_RUN);
                            }
                        } else if(bits&BIT_IS_AP_CLIENT){
                            open_sesion = true;
                        } else if(timeout>600){
                            device_clear_state(BIT_SERVER_RUN);
                        } else {
                            timeout += 1;
                        }
                        vTaskDelay(100/portTICK_PERIOD_MS);
                    }
                    deinit_server();
                    device_commit_changes();
                    vTaskDelay(1000/portTICK_PERIOD_MS);
                }
            }
            device_clear_state(BIT_START_SERVER);
            bits = device_set_state(BIT_NEW_DATA);
        }

        if(bits&BIT_UPDATE_FORECAST_DATA || bits&BIT_INIT_SNTP){
            if(connect_sta(device_get_ssid(),device_get_pwd()) == ESP_OK){
                if(! (bits&BIT_IS_TIME) || bits&BIT_INIT_SNTP ){
                    init_sntp();
                    device_wait_bits(BIT_IS_TIME);
                    bits = device_clear_state(BIT_INIT_SNTP);
                }
                if(bits&BIT_UPDATE_FORECAST_DATA){
                    if(get_weather(device_get_city_name(),device_get_api_key()) == ESP_OK){
                        if(! (bits&BIT_FORECAST_OK) ){
                            create_periodic_task(update_forecast_handler, 60*30, FOREVER);
                        }
                        device_set_state(BIT_FORECAST_OK|BIT_NEW_DATA);
                    } else {
                        if(bits&BIT_FORECAST_OK){
                            fail_count = 5;
                        }
                        create_periodic_task(update_forecast_handler, fail_count*60, FOREVER);
                        if(fail_count < 30){
                            fail_count += 5;
                        }
                        device_clear_state(BIT_FORECAST_OK);
                    }
                }
            } else {
                if(bits&BIT_FORECAST_OK){
                    fail_count = 1;
                }
                if(fail_count < 25){
                    create_periodic_task(update_forecast_handler, fail_count*60, 1);
                    fail_count += fail_count/2 + 1;
                }
                device_clear_state(BIT_FORECAST_OK);
            }
            device_clear_state(BIT_UPDATE_FORECAST_DATA);
        }
        device_clear_state(BIT_WAIT_PROCCESS);
    }
}



int tasks_init()
{
    if(xTaskCreate(
            service_task, 
            "service",
            20000, 
            NULL, 
            3,
            NULL) != pdTRUE
        || xTaskCreate(
            main_task, 
            "main",
            20000, 
            NULL, 
            3,
            NULL) != pdTRUE 
    ){
        ESP_LOGI("","task create failure");
        return ESP_FAIL;
    }
    return ESP_OK;   
}




static int timer_counter = 0;

void time_periodic_task()
{
    timer_counter -= 1;
    device_set_state(BIT_NEW_MIN);
}

static void timer_func(int cmd)
{
    static int init_val = 0;
    static bool timer_run = false;
    float t;

    if(cmd == CMD_INC || cmd == CMD_DEC) {
        init_val = timer_counter += get_encoder_val();
        reset_encoder_val();
    } else if(cmd == CMD_PRESS){
        timer_run = !timer_run;
        if(timer_run){
            restart_timer();
            create_periodic_task(time_periodic_task, 60, FOREVER);
        } else {
            remove_task(time_periodic_task);
        }
    }

    dht20_read_data(&t, NULL);
    lcd_printf_centered(15,FONT_SIZE_9, COLORED, "%s %.1fC*", snprintf_time("%H:%M"), t);
    
    if(timer_run){
        if(timer_counter <= 0){
            remove_task(time_periodic_task);
            start_alarm();
            vTaskDelay(2000/portTICK_PERIOD_MS);
            create_periodic_task(time_periodic_task, 60, FOREVER);
            timer_counter = init_val;
        }
    }

    if(timer_counter){
        lcd_printf_centered(30, FONT_SIZE_18, COLORED, "%i", timer_counter);
        lcd_print_centered_str(50, FONT_SIZE_9, COLORED, timer_run ? "min" : "Pausa");
    } else {
        lcd_print_centered_str(30, FONT_SIZE_18, COLORED, "Stop");
    }
}


static void setting_func(int cmd)
{
    unsigned bits = device_get_state();
    
    if(cmd == CMD_UPDATE_ROT_VAL){
        cmd = get_encoder_val() > 0 ? CMD_INC : CMD_DEC;
        reset_encoder_val();
    }
    if(bits&BIT_SERVER_RUN){
        if(cmd == CMD_INC){
            device_clear_state(BIT_SERVER_RUN|BIT_START_SERVER);
        }
        lcd_print_str(5, 10, FONT_SIZE_9, COLORED, "Server run!");
        lcd_print_str(5, 12, FONT_SIZE_9, COLORED, "http://192.168.4.1");
        lcd_print_str(5, 24, FONT_SIZE_9, COLORED, "SSID:" CONFIG_WIFI_AP_SSID);
        lcd_print_str(5, 36, FONT_SIZE_9, COLORED, "Password:" CONFIG_WIFI_AP_PASSWORD);
    } else {
        if(cmd == CMD_DEC){
            device_set_state(BIT_START_SERVER|BIT_WAIT_PROCCESS);
        }
        lcd_print_str(5, 12, FONT_SIZE_9, COLORED, "Press button");
        lcd_print_str(5, 24, FONT_SIZE_9, COLORED, "for starting");
        lcd_print_str(5, 36, FONT_SIZE_9, COLORED, "settings server");
    }
}


static void main_func(int cmd)
{
    float t;
    unsigned bits = device_get_state();

    if(cmd == CMD_DEC || cmd == CMD_INC){
        device_set_state(BIT_UPDATE_FORECAST_DATA);
    }

    if(cmd == CMD_UPDATE_TIME && adc_reader_get_voltage() < 3.5f){
        lcd_printf(1, 1, FONT_SIZE_9, COLORED, "B");
    }
    if(dht20_read_data(&t, NULL) == ESP_OK){
        lcd_printf(10, 7, FONT_SIZE_9, COLORED, "%.1fC*", t);
        lcd_draw_house(9, 6, 49, 10, COLORED);
    }
    if(bits & BIT_FORECAST_OK){
        lcd_printf(65, 8, FONT_SIZE_9, COLORED, "%.1fC*", service_data.temp_list[0]);
        lcd_print_centered_str(20, FONT_SIZE_9, COLORED, service_data.desciption);
    }
    if(bits & BIT_IS_TIME) {
        lcd_print_centered_str(30, FONT_SIZE_18, COLORED, snprintf_time("%H:%M"));
        lcd_print_centered_str(50, FONT_SIZE_9, COLORED, snprintf_time("%d %a"));
    } else {
        lcd_print_centered_str(35, FONT_SIZE_18, COLORED, snprintf_time("--:--"));
    }
}


static void device_info_func(int cmd)
{
    unsigned bits = device_get_state();

    lcd_printf(5, 7, FONT_SIZE_9, COLORED, "Battery:%.2fV", adc_reader_get_voltage());
    if(bits&BIT_IS_STA_CONNECTION){
        lcd_print_str(5, 20, FONT_SIZE_9, UNCOLORED, "STA:enable");
    } else {
        lcd_print_str(5, 20, FONT_SIZE_9, COLORED, "STA:disable");
    }
    if(bits&BIT_IS_AP_CONNECTION){
        lcd_print_str(5, 30, FONT_SIZE_9, UNCOLORED, "AP:enable");
    } else {
        lcd_print_str(5, 30, FONT_SIZE_9, COLORED, "AP:disable");
    }
    if(bits&BIT_SNTP_OK){
        lcd_print_str(5, 40, FONT_SIZE_9, UNCOLORED, "SNTP:enable");
    } else {
        lcd_print_str(5, 40, FONT_SIZE_9, COLORED, "SNTP:disable");
    }
    if(bits&BIT_FORECAST_OK){
        lcd_print_str(5, 50, FONT_SIZE_9, UNCOLORED, "Openweath.:Ok");
    } else {
        lcd_print_str(5, 50, FONT_SIZE_9, COLORED, "Openweath.:Nok");
    }
}


static void weather_info_func(int cmd)
{
    int dt = service_data.update_data_time;

    if(cmd == CMD_INC || cmd == CMD_DEC){
        device_set_state(BIT_UPDATE_FORECAST_DATA);
    }
    if(device_get_state()&BIT_FORECAST_OK ){
        lcd_print_centered_str(1, FONT_SIZE_9, COLORED, service_data.desciption);
    } else {
        lcd_printf_centered(1, FONT_SIZE_9, COLORED, "Last update %d:00!", dt);
    }
    for(int i=0; i<BRODCAST_LIST_SIZE; ++i){
        if(dt>23)dt %= 24;
        lcd_printf(dt >9 ? 1 : 9, 12+i*10, FONT_SIZE_9, COLORED, "%d:00   %.1fC*  %d%%", 
                            dt,
                            service_data.temp_list[i],
                            service_data.pop_list[i]);
        dt += 3;
    }
}









