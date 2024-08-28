#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "device_common.h"
#include "lcd.h"
#include "dht20.h"
#include "sound_generator.h"



#define PIN_LCD_EN          26






void app_main(void)
{
    device_common_init();
    start_signale_series(40, 3, 1000);
    vTaskDelay(pdMS_TO_TICKS(500));
    lcd_fill(UNCOLORED);
    lcd_draw_circle(64, 32, 20, COLORED);
    lcd_draw_rectangle(20, 20, 40, 40, COLORED);
    lcd_update();
    vTaskDelay(pdMS_TO_TICKS(4000));
    device_set_pin(PIN_LCD_EN, 0);

    float temp, hum;
    int i = 0;
    while (1) {

        lcd_printf(10, 10, 9, COLORED, "work! %d", i++);
        if(dht20_read_data(&temp, &hum) == ESP_OK){
            lcd_printf(10, 10, 9, COLORED, "%.1fC*", temp);
            lcd_printf(10, 40, 9, COLORED, "%.1f%%", hum);
        }
        lcd_update();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
