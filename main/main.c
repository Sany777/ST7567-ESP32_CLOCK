#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "device_common.h"
#include "sound_generator.h"
#include "screen_handler.h"
#include "voltage_controller.h"
#include "periodic_task.h"



void app_main(void)
{
    device_init_timer();
    start_signale_series(30, 4, 1500);
    device_init();
    voltage_reader_init();
    tasks_init();
}
