#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "device_common.h"
#include "lcd.h"
#include "sound_generator.h"
#include "screen_handler.h"






void app_main(void)
{
    device_init();
    start_signale_series(40, 3, 1000);
    
    vTaskDelay(pdMS_TO_TICKS(500));;
    tasks_init();
   
}
