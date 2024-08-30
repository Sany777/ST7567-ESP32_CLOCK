#include "device_common.h"
#include "sound_generator.h"

#include "periodic_task.h"
#include "device_macro.h"
#include "freertos/FreeRTOS.h"
#include "portmacro.h"
#include "esp_log.h"

#include "esp_timer.h"
#include "driver/gpio.h"


static void end_wait_but_inp();

volatile int encoder_value = 0;


int get_encoder_val() 
{
    return encoder_value;
}

void reset_encoder_val() 
{
    encoder_value = 0;
}

int get_but_state()
{
    return !gpio_get_level(PIN_ENCODER_BUT);
}


static void IRAM_ATTR encoder_isr_handler(void* arg) 
{
    static volatile int last_a = 0, last_b = 0, counter = 0;
    int a = gpio_get_level(PIN_ENCODER_PIN_A);
    int b = gpio_get_level(PIN_ENCODER_PIN_B);
    if (a != last_a || b != last_b) {
        if(counter < 4){
            counter += 1;
        } else {
            if (a == last_b) { 
                encoder_value++;
            } else {           
                encoder_value--;
            }
            device_set_state_isr(BIT_ENCODER_ROTATE|BIT_WAIT_BUT_INPUT);
            create_periodic_isr_task(end_wait_but_inp, 3000, 1);
            counter = 0;
        }
    }
    last_a = a;
    last_b = b;
}

static int but_count;



static void end_wait_but_inp()
{
    device_clear_state_isr(BIT_WAIT_BUT_INPUT);
}



static void end_but_input()
{
    if(gpio_get_level(PIN_ENCODER_BUT) || but_count > 2){
        if(but_count > 2){
            start_signale_series(40, 3, 2000);
            device_set_state_isr(BIT_BUT_LONG_PRESSED);
        } else {
            start_single_signale(50, 2000);
            device_set_state_isr(BIT_BUT_PRESSED);
        }
        remove_isr_task(end_but_input);
        create_periodic_isr_task(end_wait_but_inp, 3000, 1);
    } else {
        but_count += 1;
    }
}


static void IRAM_ATTR button_isr_handler(void* arg) 
{
    but_count = 0;
    device_set_state_isr(BIT_WAIT_BUT_INPUT);
    create_periodic_isr_task(end_but_input, 250, FOREVER);
}

void init_encoder(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_ENCODER_PIN_A) | (1ULL << PIN_ENCODER_PIN_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf);

    gpio_config_t but_conf = {
        .pin_bit_mask = (1ULL << PIN_ENCODER_BUT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&but_conf);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(PIN_ENCODER_PIN_A, encoder_isr_handler, NULL);
    gpio_isr_handler_add(PIN_ENCODER_PIN_B, encoder_isr_handler, NULL);
    gpio_isr_handler_add(PIN_ENCODER_BUT, button_isr_handler, NULL);
}

int IRAM_ATTR device_set_pin(int pin, unsigned state) 
{
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT_OUTPUT);
    return gpio_set_level((gpio_num_t)pin, state);
}
