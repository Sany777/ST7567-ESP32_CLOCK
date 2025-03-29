#include "device_task.h"

#include "esp_timer.h"
#include "sdkconfig.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "portmacro.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "stdbool.h"

#include "periodic_task.h"
#include "device_common.h"
#include "forecast_http_client.h"
#include "sound_generator.h"
#include "dht20.h"
#include "adc_reader.h"
#include "toolbox.h"
#include "wifi_service.h"
#include "lcd.h"
#include "clock_module.h"
#include "setting_server.h"

enum FuncId{
    SCREEN_MAIN,
    SCREEN_TIMER,
    SCREEN_SETTING,
    SCREEN_DEVICE_INFO,
    SCREEN_FORECAST_DETAIL,
};

enum TimeoutMS{
    TIMEOUT_SEC             = 1000,
    TIMEOUT_2_SEC           = 2*TIMEOUT_SEC,
    TIMEOUT_5_SEC           = 5*TIMEOUT_SEC,
    TIMEOUT_BUT_INP         = 5*TIMEOUT_SEC,
    TIMEOUT_6_SEC           = 6*TIMEOUT_SEC,
    TIMEOUT_20_SEC          = 20*TIMEOUT_SEC,
    TIMEOUT_MINUTE          = 60*TIMEOUT_SEC,
    TIMEOUT_FOUR_MINUTE     = 4*TIMEOUT_MINUTE,
    TIMEOUT_HOUR            = 60*TIMEOUT_MINUTE,
    DELAY_TRY_GET_DATA      = 2*TIMEOUT_MINUTE,
    DELAY_UPDATE_FORECAST   = 32*TIMEOUT_MINUTE,
    INTERVAL_CHECK_BAT      = TIMEOUT_MINUTE * 10,
    INTERVAL_UPDATE_TIME    = 8*TIMEOUT_HOUR,
    LOW_BAT_SIG_DELAY       = TIMEOUT_MINUTE * 10
};

enum TaskDelay{
    DELAY_SERV      = 100,
    DELAY_MAIN_TASK = 100,
};

static void timer_func(int cmd);
static void setting_func(int cmd);
static void main_func(int cmd);
static void device_info_func(int cmd);
static void weather_info_func(int cmd);


typedef void(*handler_func_t)(int);

static const handler_func_t func_list[] = {
    main_func,
    timer_func,
    setting_func,
    device_info_func,
    weather_info_func,
};

static const int SCREEN_LIST_SIZE = sizeof(func_list)/sizeof(func_list[0]);

enum {

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
static float temp;
static int timer_counter = 0;
static bool timer_run;
static float volt_val;
static long long start_task_time;

static void update_forecast_handler();
static void timer_counter_handler();
static void check_bat_status_handler();
static void update_time_handler();
static void low_bat_signal_handler();



static void main_task(void *pv)
{
    unsigned bits;
    int screen = NO_DATA;
    int cmd = NO_DATA;
    long long sleep_time_ms;
    int timeout = TIMEOUT_BUT_INP;
    unsigned time_work = 0;
    set_offset(device_get_offset());
    const struct tm * tinfo = get_cur_time_tm();
    next_screen = SCREEN_MAIN;
    device_set_pin(PIN_LCD_BACKLIGHT_EN, 0);
    lcd_init();
    device_set_state(BIT_UPDATE_FORECAST_DATA);
    create_periodic_task(check_bat_status_handler, TIMEOUT_MINUTE * 2, FOREVER);
    create_periodic_task(update_time_handler, INTERVAL_UPDATE_TIME, FOREVER);
    bool backlight_en = false, task_run, but_pressed = true;
    float cur_volt_val;
    vTaskDelay(500/portTICK_PERIOD_MS);
    for(;;){

        task_run = true;
        device_start_timer();
        start_task_time = esp_timer_get_time();
        device_set_pin(PIN_DHT20_EN, 1);
        if(dht20_wait() == ESP_OK){
            dht20_read_data(&temp, NULL);
        }
        device_set_pin(PIN_DHT20_EN, 0);
        do{
            if(but_pressed) 
                vTaskDelay(100/portTICK_PERIOD_MS);
            bits = device_get_state();
            if(bits&BIT_EVENT_BUT_LONG_PRESSED){
                start_single_signale(120, 2000);
                backlight_en = !backlight_en;
                device_set_pin(PIN_LCD_BACKLIGHT_EN, backlight_en);
                device_clear_state(BIT_EVENT_BUT_LONG_PRESSED);
            } 
            if(screen != next_screen) {
                if(next_screen >= SCREEN_LIST_SIZE){
                    next_screen = 0;
                } else if(next_screen < 0){
                    next_screen = SCREEN_LIST_SIZE-1;
                }
                screen = next_screen;
                cmd = CMD_INIT;
            } else if(bits&BIT_EVENT_ENCODER_ROTATE){
                start_single_signale(75, 2500);
                if(get_encoder_val() > 0){
                    cmd = CMD_INC;
                } else {
                    cmd = CMD_DEC;
                }
                device_clear_state(BIT_EVENT_ENCODER_ROTATE);
            } else if(bits&BIT_EVENT_BUT_PRESSED){
                start_single_signale(50, 2000);
                cmd = CMD_PRESS;
                device_clear_state(BIT_EVENT_BUT_PRESSED);
            } else if(bits&BIT_EVENT_NEW_DATA) {
                cmd = CMD_UPDATE_DATA;
                device_clear_state(BIT_EVENT_NEW_DATA);
                cmd = CMD_UPDATE_DATA; 
            } else if(bits&BIT_EVENT_NEW_MIN) {
                if(screen == SCREEN_MAIN){
                    start_task_time = esp_timer_get_time();
                    if(bits&BIT_IS_TIME 
                        && bits&BIT_NOTIF_ENABLE
                            && is_signale(tinfo)){
                        start_signale_series(100, 5, 2000);
                    }
                }
                cmd = CMD_UPDATE_DATA;
                device_clear_state(BIT_EVENT_NEW_MIN);
            } else if(bits&BIT_EVENT_NEW_T_MIN) {
                if(screen == SCREEN_TIMER){
                    cmd = CMD_UPDATE_TIME;
                }
                device_clear_state(BIT_EVENT_NEW_T_MIN);
            } else if(bits&BIT_CHECK_BAT) {
                cur_volt_val = device_get_voltage();
                if( ! ((cur_volt_val - volt_val) > 0.2) && cur_volt_val < ALARM_VOLTAGE){
                    if(cur_volt_val < MIN_VOLTAGE
                        || (bits&BIT_IS_TIME && ! is_signal_allowed(tinfo))){
                        device_set_pin(PIN_LCD_BACKLIGHT_EN, 0);
                        device_set_pin(PIN_DHT20_EN, 0);
                        vTaskDelay(1000/portTICK_PERIOD_MS);
                        esp_deep_sleep(UINT64_MAX);
                    }
                    if(! (bits&BIT_EVENT_IS_LOW_BAT)){
                        device_set_state(BIT_EVENT_IS_LOW_BAT);
                        cmd = CMD_UPDATE_DATA;
                        create_periodic_task(low_bat_signal_handler, LOW_BAT_SIG_DELAY, FOREVER);
                    }
                } else if(bits&BIT_EVENT_IS_LOW_BAT){
                    device_clear_state(BIT_EVENT_IS_LOW_BAT);
                    cmd = CMD_UPDATE_DATA;
                    remove_task(low_bat_signal_handler);
                }
                volt_val = cur_volt_val;
                device_clear_state(BIT_CHECK_BAT);
            }else if( ! (bits&BITS_DENIED_SLEEP) && ! (bits&BIT_WAIT_SIGNALE)){
                time_work = (esp_timer_get_time() - start_task_time) / 1000;
                if(time_work > timeout){
                    task_run = false;
                    cmd = CMD_UPDATE_DATA;
                }
            }
            if(cmd != NO_DATA){
                lcd_fill(UNCOLORED);
                func_list[screen](cmd);
                if(screen == next_screen){
                    lcd_update();
                }
                if(cmd == CMD_DEC || cmd == CMD_INC){
                    reset_encoder_val();
                }
                cmd = NO_DATA;
            } 
        }while(task_run);

        if(backlight_en){
            device_set_pin(PIN_LCD_BACKLIGHT_EN, 0);
            backlight_en = false;
        }
        if(tinfo->tm_hour < 5){
            sleep_time_ms = TIMEOUT_FOUR_MINUTE - time_work%TIMEOUT_MINUTE;
        } else {
            sleep_time_ms = TIMEOUT_MINUTE - time_work%TIMEOUT_MINUTE;
        }
        esp_sleep_enable_timer_wakeup(sleep_time_ms * 1000);
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_WAKEUP, 0);
        device_stop_timer();
        esp_light_sleep_start(); 
        if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER){
            timeout = 1;
            device_set_state(BIT_EVENT_NEW_MIN);
            if(!timer_run){
                next_screen = SCREEN_MAIN;
            }
            but_pressed = false;
        } else {
            but_pressed = true;
            timeout = TIMEOUT_BUT_INP;
        }
    }
}




static void service_task(void *pv)
{
    uint32_t bits;
    bool open_sesion;
    int delay_update_forecast = DELAY_TRY_GET_DATA;
    vTaskDelay(100/portTICK_PERIOD_MS);
    int wait_client_timeout;
    for(;;){
        bits = device_wait_bits_untile(BIT_UPDATE_FORECAST_DATA|BIT_START_SERVER|BIT_FORCE_UPDATE_FORECAST_DATA, 
                            portMAX_DELAY);
        device_set_state(BITS_DENIED_SLEEP);
        if(bits & BIT_START_SERVER){
            if(start_ap() == ESP_OK){
                vTaskDelay(1500/portTICK_PERIOD_MS);
                if(init_server(network_buf) == ESP_OK){
                    wait_client_timeout = 0;
                    device_set_state(BIT_SERVER_RUN);
                    open_sesion = false;
                    device_set_state(BIT_EVENT_NEW_DATA);
                    while((bits = device_get_state()) & BIT_SERVER_RUN){
                        if(open_sesion){
                            if(!(bits&BIT_IS_AP_CLIENT) ){
                                device_clear_state(BIT_SERVER_RUN);
                            }
                        } else if(bits&BIT_IS_AP_CLIENT){
                            wait_client_timeout = 0;
                            open_sesion = true;
                        } else if(wait_client_timeout>TIMEOUT_MINUTE){
                            device_clear_state(BIT_SERVER_RUN);
                        } else {
                            wait_client_timeout += DELAY_SERV;
                        }
                        vTaskDelay(DELAY_SERV/portTICK_PERIOD_MS);

                    }
                    deinit_server();
                    bool changed_settings = device_commit_changes();
                    if(changed_settings && ! (bits&BIT_FORECAST_OK) ){
                        bits = device_set_state(BIT_UPDATE_FORECAST_DATA);
                    }
                }
                wifi_stop();
            }
            device_clear_state(BIT_START_SERVER);
        }

        if(bits&BIT_UPDATE_FORECAST_DATA || bits&BIT_FORCE_UPDATE_FORECAST_DATA){
            if(connect_sta(device_get_ssid(), device_get_pwd()) == ESP_OK){
                device_set_state(BIT_STA_CONF_OK);
                if(bits&BIT_UPDATE_TIME || !(bits&BIT_IS_TIME)){
                    vTaskDelay(500/portTICK_PERIOD_MS);
                    if(device_update_time()){
                        bits = device_set_state(BIT_IS_TIME);
                    } else {
                        init_sntp();
                        bits = device_wait_bits(BIT_IS_TIME);
                    }
                    if(bits&BIT_IS_TIME){
                        device_clear_state(BIT_UPDATE_TIME);  
                    }
                }
                vTaskDelay(500/portTICK_PERIOD_MS);
                if(update_forecast_data(device_get_city_name(),device_get_api_key())){
                    service_data.update_data_time = get_cur_time_tm()->tm_hour;
                    if(! (bits&BIT_FORECAST_OK)){
                        delay_update_forecast = DELAY_UPDATE_FORECAST;
                        device_set_state(BIT_FORECAST_OK);
                        create_periodic_task(update_forecast_handler, DELAY_UPDATE_FORECAST, FOREVER);
                    }
                } else {
                    device_clear_state(BIT_FORECAST_OK);
                    if(bits&BIT_UPDATE_FORECAST_DATA){
                        create_periodic_task(update_forecast_handler, delay_update_forecast, FOREVER);
                        if(delay_update_forecast < DELAY_UPDATE_FORECAST){
                            delay_update_forecast *= 2;
                        }
                    }
                }
            }
        }
        wifi_stop();
        device_set_state(BIT_EVENT_NEW_DATA);
        vTaskDelay(500/portTICK_PERIOD_MS);
        device_clear_state(BIT_UPDATE_FORECAST_DATA|BIT_FORCE_UPDATE_FORECAST_DATA|BITS_DENIED_SLEEP);
    }
}



void task_init()
{
    xTaskCreate(
            service_task, 
            "service",
            20000, 
            NULL, 
            3,
            NULL);
    xTaskCreate(
            main_task, 
            "main",
            20000, 
            NULL, 
            4,
            NULL);
}


void print_temp_indoor()
{
    lcd_printf(16, 51, FONT_SIZE_9, COLORED, "%.0fC*", temp);
    lcd_draw_house(15, 51, 40, 10, COLORED);
    lcd_draw_line(0, 61, 128, COLORED, HORISONTAL, 0);
    lcd_draw_line(1, 62, 127, COLORED, HORISONTAL, 1);
    lcd_draw_line(0, 63, 128, COLORED, HORISONTAL, 1);
}

static void timer_func(int cmd)
{
    static int init_val = 0;

    if(cmd == CMD_INIT){
        timer_run = false;
        timer_counter = init_val = 1;
    } else if(cmd == CMD_INC || cmd == CMD_DEC) {
        if(timer_run){
            timer_run = false;
            remove_task(timer_counter_handler);
        }
        init_val = timer_counter += get_encoder_val();
        if(init_val <= 0){
            next_screen = SCREEN_MAIN;
            return;
        }
    } else if(cmd == CMD_PRESS){
        if(init_val > 0){
            timer_run = !timer_run;
            if(timer_run){
                timer_counter = init_val-1;
                start_task_time = esp_timer_get_time();
                create_periodic_task(timer_counter_handler, TIMEOUT_MINUTE, FOREVER);
            } else {
                remove_task(timer_counter_handler);
            }
        }
    }

    print_temp_indoor();
    lcd_print_str(70, 50, FONT_SIZE_9, COLORED, snprintf_time("%H:%M"));

    if(cmd == CMD_UPDATE_TIME && timer_run && timer_counter == 0){
        timer_run = false;
        lcd_print_centered_str(20, FONT_SIZE_18, COLORED, "0");
        lcd_update();
        start_alarm();
        lcd_fill(UNCOLORED);
        print_temp_indoor();
        lcd_print_str(70, 50, FONT_SIZE_9, COLORED, snprintf_time("%H:%M"));
        int timeout = 50;
        int light_en = 0;
        do{
            vTaskDelay(100/portTICK_PERIOD_MS);
            if(timeout%5){
                light_en = !light_en;
                device_set_pin(PIN_LCD_BACKLIGHT_EN, light_en);
            }
            if(get_but_state()){
                sound_off();
                device_set_pin(PIN_LCD_BACKLIGHT_EN, 0);
                do{
                    vTaskDelay(100/portTICK_PERIOD_MS);
                }while(get_but_state());
                device_clear_state(BIT_EVENT_BUT_PRESSED|BIT_EVENT_BUT_LONG_PRESSED);
                next_screen = SCREEN_MAIN;
                return;
            }
            timeout -= 1;
        }while(timeout);
        device_set_pin(PIN_LCD_BACKLIGHT_EN, 0);
        timer_run = true;
        timer_counter = init_val;
        create_periodic_task(timer_counter_handler, TIMEOUT_MINUTE, FOREVER);
    }

    lcd_printf_centered(15, FONT_SIZE_18, COLORED, "%i", timer_counter);
    lcd_print_centered_str(35, FONT_SIZE_9, COLORED, timer_run ? "min" : "Pausa");
}


static void setting_func(int cmd)
{
    if(cmd == CMD_INC || cmd == CMD_DEC){
        next_screen +=  cmd == CMD_INC ? 1 : -1;
        device_clear_state(BIT_SERVER_RUN|BIT_START_SERVER);
        return;
    }

    unsigned bits = device_get_state();
    
    lcd_draw_line(1, 0, 127, COLORED, HORISONTAL, 1);
    lcd_draw_line(0, 1, 128, COLORED, HORISONTAL, 1);
    lcd_draw_line(1, 2, 127, COLORED, HORISONTAL, 1);
    lcd_draw_line(1, 61, 127, COLORED, HORISONTAL, 1);
    lcd_draw_line(0, 62, 128, COLORED, HORISONTAL, 1);
    lcd_draw_line(1, 63, 127, COLORED, HORISONTAL, 1);


    if(bits&BIT_SERVER_RUN){
        if(cmd == CMD_PRESS){
            device_clear_state(BIT_SERVER_RUN|BIT_START_SERVER);
        }
        lcd_print_centered_str(2, FONT_SIZE_9, COLORED, "Server run!");
        lcd_print_centered_str(12, FONT_SIZE_9, COLORED, "http://");
        lcd_print_centered_str(22, FONT_SIZE_9, COLORED,"192.168.4.1");
        lcd_print_centered_str(32, FONT_SIZE_9, COLORED, "SSID:" CONFIG_WIFI_AP_SSID);
        lcd_print_centered_str(42, FONT_SIZE_9, COLORED, "Password:");
        lcd_print_centered_str(52, FONT_SIZE_9, COLORED, CONFIG_WIFI_AP_PASSWORD);
    } else {
        if(cmd == CMD_PRESS){
            device_set_state(BIT_START_SERVER);
        }
        lcd_print_centered_str(12, FONT_SIZE_9, COLORED, "Press button");
        lcd_print_centered_str(24, FONT_SIZE_9, COLORED, "for starting");
        lcd_print_centered_str(36, FONT_SIZE_9, COLORED, "settings server");
    }
}


static void main_func(int cmd)
{
    int ver_desc, data_indx;

    if(cmd == CMD_INC || cmd == CMD_DEC){
        next_screen +=  cmd == CMD_INC ? 1 : -1;
        return;
    }

    const unsigned bits = device_get_state();

    if(bits&BIT_EVENT_IS_LOW_BAT){
        lcd_printf(1, 1, FONT_SIZE_9, COLORED, "%u%%", 
            battery_voltage_to_percentage(volt_val));
        lcd_draw_rectangle(0, 0, 32, 10, COLORED);
        lcd_draw_rectangle(32, 3, 4, 4, COLORED);
        ver_desc = 11;
    } else {
        ver_desc = 8;
    }
    
    print_temp_indoor();

    data_indx = get_actual_forecast_data_index(get_cur_time_tm()->tm_hour, service_data.update_data_time);

    if(data_indx != NO_DATA){
        lcd_printf(70, 49, FONT_SIZE_9, COLORED, "%dC*", service_data.temp_list[data_indx]);
        lcd_print_centered_str(ver_desc, FONT_SIZE_9, COLORED, service_data.desciption[data_indx]);
    }
    
    lcd_print_centered_str(20, FONT_SIZE_18, COLORED, snprintf_time("%H:%M"));
    lcd_print_centered_str(37, FONT_SIZE_9, COLORED, snprintf_time("%d %a"));
}



static void device_info_func(int cmd)
{

    if(cmd == CMD_INC || cmd == CMD_DEC){
        next_screen +=  cmd == CMD_INC ? 1 : -1;
        return;
    }
    
    const unsigned bits = device_get_state();
    float voltage = device_get_voltage();
    lcd_printf_centered(3, FONT_SIZE_9, COLORED, "Bat:%u%%, %.2fV", 
                        battery_voltage_to_percentage(voltage), 
                        voltage);

    lcd_draw_line(1, 16, 127, COLORED, HORISONTAL, 1);
    lcd_draw_line(0, 17, 128, COLORED, HORISONTAL, 1);
    lcd_draw_line(1, 18, 127, COLORED, HORISONTAL, 1);

    if(bits&BIT_IS_STA_CONNECTION){
        lcd_print_str(2, 20, FONT_SIZE_9, COLORED, "WiFi:connected");
    } else if(bits&BIT_ERR_SSID_NOT_FOUND){
        lcd_print_str(2, 20, FONT_SIZE_9, UNCOLORED, "SSID:not found");
    } else if( ! (bits&BIT_STA_CONF_OK)){
        lcd_print_str(2, 20, FONT_SIZE_9, UNCOLORED, "WiFi pwd is wrong");
    } else {
        lcd_print_str(2, 20, FONT_SIZE_9, COLORED, "WiFi:disable");
    }
    if(bits&BIT_IS_AP_CLIENT){
        lcd_print_str(2, 30, FONT_SIZE_9, COLORED, "AP:is a client");
    }else if(bits&BIT_IS_AP_CLIENT){
        lcd_print_str(2, 30, FONT_SIZE_9, COLORED, "AP client");
    } else {
        lcd_print_str(2, 30, FONT_SIZE_9, COLORED, "no AP client");
    }
    if(bits&BIT_IS_TIME){
        lcd_print_str(2, 40, FONT_SIZE_9, COLORED, "Time:Ok");
    } else {
        lcd_print_str(2, 40, FONT_SIZE_9, UNCOLORED, "Time:NOk");
    }
    if(bits&BIT_FORECAST_OK){
        lcd_print_str(2, 50, FONT_SIZE_9, COLORED, "Openweath.:Ok");
    } else {
        lcd_print_str(2, 50, FONT_SIZE_9, UNCOLORED, "Openweath.:Nok");
    }
}


static void weather_info_func(int cmd)
{
    int data_indx, dt;

    if(cmd == CMD_INC || cmd == CMD_DEC){
        next_screen += cmd == CMD_INC ? 1 : -1;
        return;
    }
    lcd_draw_line(0, 10, 128, COLORED, HORISONTAL, 1);
    lcd_draw_line(1, 11, 127, COLORED, HORISONTAL, 1);
    if(cmd == CMD_PRESS){
        device_clear_state(BIT_IS_TIME);
        device_set_state(BIT_FORCE_UPDATE_FORECAST_DATA);
    }

    dt = service_data.update_data_time;

    if(dt == NO_DATA){
        lcd_print_centered_str(20, FONT_SIZE_9, COLORED, "The data has not");
        lcd_print_centered_str(40, FONT_SIZE_9, COLORED, " been updated yeat");
    } else {

        data_indx = get_actual_forecast_data_index(get_cur_time_tm()->tm_hour, dt);

        if(data_indx == NO_DATA){
            lcd_print_centered_str(20, FONT_SIZE_9, COLORED, "Data has been updated");
            lcd_printf_centered(40, FONT_SIZE_9, COLORED, "%d:00", dt);
        } else {
            lcd_print_centered_str(1, FONT_SIZE_9, COLORED, service_data.desciption[data_indx]);
            for(int i=0; i<FORECAST_LIST_SIZE; ++i){
                if(dt>23) dt %= 24;
                lcd_printf(dt>9 ? 1 : 9, 
                            14+i*10, 
                            FONT_SIZE_9, 
                            COLORED, 
                            "%d:00", dt);
                lcd_printf(service_data.temp_list[i]/10 ? 45 : 50, 
                                14+i*10, 
                                FONT_SIZE_9, 
                                COLORED, 
                                "%dC*", service_data.temp_list[i]);
                lcd_printf(service_data.pop_list[i]/10 ? 95 : 100, 
                            14+i*10, 
                            FONT_SIZE_9, 
                            COLORED, 
                            "%d%%", service_data.pop_list[i]);
                dt += 3;
            }
            lcd_draw_line(42, 12, 64, COLORED, VERTICAL, 1);
            lcd_draw_line(90, 12, 64, COLORED, VERTICAL, 1);
        }
    }
}

static void update_forecast_handler()
{
    device_set_state_isr(BIT_UPDATE_FORECAST_DATA);
}


static void timer_counter_handler()
{
    if(timer_counter > 0){
        timer_counter -= 1;
    } else {
        remove_task_isr(timer_counter_handler);
    }
    device_set_state_isr(BIT_EVENT_NEW_T_MIN); 
}

static void check_bat_status_handler()
{
    device_set_state_isr(BIT_CHECK_BAT);
    create_periodic_task(check_bat_status_handler, INTERVAL_CHECK_BAT, 1);
}

static void update_time_handler()
{
    device_set_state_isr(BIT_UPDATE_TIME);
}

static void low_bat_signal_handler()
{
    start_signale_series(100, 10, 2000);
}

