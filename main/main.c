#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "device_common.h"
#include "sound_generator.h"
#include "screen_handler.h"






void app_main(void)
{
    device_init();
    start_signale_series(30, 4, 1500);
    tasks_init();
}
