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



static void timer_func(int cmd_id);
static void setting_func(int cmd_id);
static void main_func(int cmd_id);
static void device_info_func(int cmd_id);
static void weather_info_func(int cmd_id);


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
    CMD_INC = BUT_SCC,
    CMD_PRESS = BUT_SWW,
    CMD_DEC = BUT_DT,
    CMD_INIT,
    CMD_DEINIT,
    CMD_UPDATE_DATA,
    CMD_UPDATE_TIME,
    CMD_UPDATE_POS,
    CMD_UPDATE_TIMER_TIME
};

static int next_screen;




static void main_task(void *pv)
{
    unsigned bits;
    int cmd;

    int screen = -1;
    uint64_t sleep_time_ms, working_time;
    next_screen = SCREEN_MAIN;
    bool but_input, exit;
    int timeout = TIMEOUT_6_SEC;
    device_set_state(BIT_UPDATE_FORECAST_DATA);
    vTaskDelay(100/portTICK_PERIOD_MS);

    for(;;){

        but_input = exit = false;
        restart_timer();
        do{
            cmd = device_get_joystick_btn();

            if(cmd != NO_DATA){
                if(!but_input){
                    but_input = true;
                } else if(cmd == CMD_PRESS){
                    ++next_screen;
                }
            }

            bits = device_get_state();



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
            } else if( ! (bits&BIT_WAIT_SIGNALE)
                        && ! (bits&BIT_WAIT_BUT_INPUT)
                            && ! (bits&BIT_WAIT_PROCCESS)
                                && get_timer_ms() > timeout){
                exit = true;
                cmd = CMD_UPDATE_DATA;
            }
            
            if(cmd != NO_DATA){
                // epaper_clear(UNCOLORED); 
                func_list[screen](cmd);
                bits = device_get_state();
                if(exit){
                    // epaper_display_all();
                } else {
                    // epaper_display_part();
                }
                vTaskDelay(250/portTICK_PERIOD_MS);
                // epaper_wait();
            }

            vTaskDelay(250/portTICK_PERIOD_MS);

        } while(! exit);

        wifi_stop();
        

            sleep_time_ms = ONE_MINUTE - get_timer_ms()%ONE_MINUTE;
            device_set_pin(DHT20_EN_PIN, 0);
            esp_sleep_enable_timer_wakeup(sleep_time_ms * 1000);
            esp_sleep_enable_ext0_wakeup((gpio_num_t)GPIO_WAKEUP_PIN, 1);
            esp_light_sleep_start();
            device_set_pin(DHT20_EN_PIN, 1);    
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

static void timer_func(int cmd_id)
{
    static int init_val = 0;
    static bool timer_run = false;
    float t;
    bool set_timer_state = timer_run;

    if(cmd_id == CMD_INC) {
        init_val = timer_counter = (init_val/5)*5 + 5;
        set_timer_state = true;
    } else if(cmd_id == CMD_DEC){
        if(init_val){
            if(init_val <= 5){
                init_val -= 1;
            } else {
                init_val = (init_val/5)*5 - 5;
            }
        }
        timer_counter = init_val;
        set_timer_state = true;
    } 



    if(timer_run != set_timer_state){
        if(set_timer_state){
            restart_timer();
            create_periodic_task(time_periodic_task, 60, FOREVER);
        }else{
            remove_task(time_periodic_task);
        }
        timer_run = set_timer_state;
    }

    // epaper_printf_centered(35, FONT_SIZE_34, COLORED, snprintf_time("%H:%M"));
    if(dht20_read_data(&t, NULL) == ESP_OK){
        // epaper_printf_centered(175, FONT_SIZE_20, COLORED, "%.1fC*", t);
    }
    
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
       
        // epaper_printf_centered(90, FONT_SIZE_64, UNCOLORED, "%i", timer_counter);
        if(timer_run){
            // epaper_print_centered_str(145, FONT_SIZE_20, UNCOLORED, "min");
        } else {
            // epaper_print_centered_str(145, FONT_SIZE_20, UNCOLORED, "Pausa");
        }
    } else {
        // epaper_print_centered_str(90, FONT_SIZE_48, COLORED, "Stop");
    }
}


static void setting_func(int cmd_id)
{
    unsigned bits = device_get_state();

    if(bits&BIT_SERVER_RUN){
        if(cmd_id == CMD_INC){
            device_clear_state(BIT_SERVER_RUN|BIT_START_SERVER);
        }
        // epaper_print_centered_str(20, FONT_SIZE_20, COLORED, "Server run!");
        // epaper_print_centered_str(40, FONT_SIZE_20, COLORED, "http://192.168.4.1");
        // epaper_print_centered_str(60, FONT_SIZE_20, COLORED, "SSID:" CONFIG_WIFI_AP_SSID);
        // epaper_print_centered_str(80, FONT_SIZE_20, COLORED, "Password:" CONFIG_WIFI_AP_PASSWORD);
    } else {
        if(cmd_id == CMD_DEC){
            device_set_state(BIT_START_SERVER|BIT_WAIT_PROCCESS);
        }
        // epaper_print_centered_str(40, FONT_SIZE_20, COLORED, "Press button");
        // epaper_print_centered_str(60, FONT_SIZE_20, COLORED, "for starting");
        // epaper_print_centered_str(80, FONT_SIZE_20, COLORED, "settings server");
    }
}


static void main_func(int cmd_id)
{
    float t, hum;
    unsigned bits = device_get_state();


    if(cmd_id == CMD_DEC || cmd_id == CMD_INC){
        device_set_state(BIT_UPDATE_FORECAST_DATA);
    }

    if(cmd_id == CMD_UPDATE_TIME && adc_reader_get_voltage() < 3.5f){
        // epaper_printf(10, 10, FONT_SIZE_16, COLORED, "BAT!");
    }

    if(dht20_read_data(&t, &hum) == ESP_OK){

    }
    if(bits & BIT_FORECAST_OK){
        // epaper_printf_centered(75, FONT_SIZE_24, COLORED, "%.1fC* R:%d%%", 
                        // service_data.temp_list[0], 
                        // (service_data.pop_list[0] + service_data.pop_list[1])/2);
        // epaper_print_centered_str(100, FONT_SIZE_34, COLORED, 
                        // service_data.desciption);
    }
    if(bits & BIT_IS_TIME) {
        // epaper_printf_centered(125, FONT_SIZE_48, COLORED, snprintf_time("%H:%M"));
        // epaper_printf_centered(172, FONT_SIZE_24, COLORED, snprintf_time("%d %a"));
    } else {
        // epaper_printf_centered(125, FONT_SIZE_48, COLORED, snprintf_time("--:--"));
    }
}


static void device_info_func(int cmd_id)
{
    unsigned bits = device_get_state();

    // epaper_printf(5, 30, FONT_SIZE_20, COLORED, "Battery:%.2fV", adc_reader_get_voltage());
    if(bits&BIT_IS_STA_CONNECTION){
        // epaper_print_str(5, 60, FONT_SIZE_20, UNCOLORED, "STA:enable");
    } else {
        // epaper_print_str(5, 60, FONT_SIZE_20, COLORED, "STA:disable");
    }
    if(bits&BIT_IS_AP_CONNECTION){
        // epaper_print_str(5, 90, FONT_SIZE_20, UNCOLORED, "AP:enable");
    } else {
        // epaper_print_str(5, 90, FONT_SIZE_20, COLORED, "AP:disable");
    }
    if(bits&BIT_SNTP_OK){
        // epaper_print_str(5, 120, FONT_SIZE_20, UNCOLORED, "SNTP:enable");
    } else {
        // epaper_print_str(5, 120, FONT_SIZE_20, COLORED, "SNTP:disable");
    }
    if(bits&BIT_FORECAST_OK){
        // epaper_print_str(5, 150, FONT_SIZE_20, UNCOLORED, "Openweath.:Ok");
    } else {
        // epaper_print_str(5, 150, FONT_SIZE_20, COLORED, "Openweath.:Nok");
    }
}


static void weather_info_func(int cmd_id)
{
    int dt = service_data.update_data_time;

    if(cmd_id == CMD_INC || cmd_id == CMD_DEC){
        device_set_state(BIT_UPDATE_FORECAST_DATA);
    }
    if(device_get_state()&BIT_FORECAST_OK ){
        // epaper_print_centered_str(15, FONT_SIZE_20, COLORED, service_data.desciption);
    } else {
        // epaper_printf_centered(15, FONT_SIZE_20, COLORED, "last update %d:00", dt);
    }
    // draw_rect(0,44,200, 61, COLORED, true);
    // epaper_print_str(10, 47, FONT_SIZE_12, UNCOLORED, "hour    temp       rain");
    for(int i=0; i<BRODCAST_LIST_SIZE; ++i){
        if(dt>23)dt %= 24;
        // epaper_printf(6, 70+i*25, FONT_SIZE_20, COLORED, "%c%c|%.1fC*|%d%%", 
                            // dt>10 ? (dt/10)+'0':' ', 
                            // (dt%10) + '0',
                            // service_data.temp_list[i],
                            // service_data.pop_list[i]);
        dt += 3;
    }
}









