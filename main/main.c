#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "device_common.h"
#include "sound_generator.h"
#include "device_task.h"
#include "adc_reader.h"
#include "periodic_task.h"



void app_main(void)
{
    device_init();
    device_init_timer();
    start_signale_series(70, 3, 1500);
    vTaskDelay(100/portTICK_PERIOD_MS);
    adc_reader_init();
    task_init();
}
