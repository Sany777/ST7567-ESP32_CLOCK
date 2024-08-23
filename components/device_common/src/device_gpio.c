#include "device_common.h"


#include "sound_generator.h"
#include "device_common.h"
#include "driver/gpio.h"
#include "periodic_task.h"
#include "device_macro.h"
#include "portmacro.h"
// 34 - UP, 27 - left, 35 - right, 32 - , 33 -center,

static const int joystic_pin[] = {GPIO_NUM_35,GPIO_NUM_33,GPIO_NUM_27};
static const int BUT_NUM = sizeof(joystic_pin)/sizeof(joystic_pin[0]);


static void IRAM_ATTR send_sig_update_pos()
{
    clear_bit_from_isr(BIT_WAIT_MOVING);
}

void IRAM_ATTR gpio_isr_handler(void* arg)
{
    set_bit_from_isr(BIT_WAIT_MOVING);
    periodic_task_isr_create(send_sig_update_pos, 300, 1);
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
    for(int i=0; i<BUT_NUM; ++i){
        gpio_set_direction(joystic_pin[i], GPIO_MODE_INPUT);
        gpio_pulldown_en(joystic_pin[i]);
    }
    setup_gpio_interrupt();
}

static void end_but_input()
{
    clear_bit_from_isr(BIT_WAIT_BUT_INPUT);
}

int device_get_joystick_btn()
{
    for(int i=0; i<BUT_NUM; ++i){
        if(gpio_get_level(joystic_pin[i])){
            start_single_signale(10, 1000);
            device_set_state(BIT_WAIT_BUT_INPUT);
            periodic_task_isr_create(end_but_input, 4000, 1);
            return i;
        }
    }
    return NO_DATA;
}