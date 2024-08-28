#include "device_common.h"


#include "sound_generator.h"
#include "device_common.h"
#include "driver/gpio.h"
#include "periodic_task.h"
#include "device_macro.h"

#include "freertos/FreeRTOS.h"
#include "portmacro.h"

// DT 12, SCC 14, SWW 33

static const int swt_pin[] = {GPIO_NUM_12,GPIO_NUM_14,GPIO_NUM_33};
static const int SWT_NUM = sizeof(swt_pin)/sizeof(swt_pin[0]);


static void IRAM_ATTR send_sig_update_pos()
{
    clear_bit_from_isr(BIT_WAIT_MOVING);
}

void IRAM_ATTR gpio_isr_handler(void* arg)
{
    set_bit_from_isr(BIT_WAIT_MOVING);
    create_periodic_isr_task(send_sig_update_pos, 300, 1);
}

void setup_gpio_interrupt()
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE, 
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << GPIO_WAKEUP_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_WAKEUP_PIN, gpio_isr_handler, NULL);
}

int IRAM_ATTR device_set_pin(int pin, unsigned state)
{
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT_OUTPUT);
    return gpio_set_level((gpio_num_t )pin, state);
}


void device_gpio_init()
{
    for(int i=0; i<SWT_NUM; ++i){
        gpio_set_direction(swt_pin[i], GPIO_MODE_INPUT);
        // gpio_pulldown_en(swt_pin[i]);
    }
    setup_gpio_interrupt();
}

static void end_but_input()
{
    clear_bit_from_isr(BIT_WAIT_BUT_INPUT);
}

int device_get_joystick_btn()
{
    for(int i=0; i<SWT_NUM; ++i){
        if(gpio_get_level(swt_pin[i])){
            start_single_signale(100, 2000);
            device_set_state(BIT_WAIT_BUT_INPUT);
            create_periodic_isr_task(end_but_input, 4000, 1);
            return i;
        }
    }
    return NO_DATA;
}

