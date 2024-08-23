#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "device_common.h"
#include "lcd.h"




#define PIN_LCD_EN      26





void app_main(void)
{
    lcd_init();
    int i = 0;
        // test_lcd();
    device_set_pin(PIN_LCD_EN, 1);
    lcd_fill(UNCOLORED);
    lcd_draw_circle(64, 32, 20, COLORED);
    lcd_draw_rectangle(20, 20, 40, 40, COLORED);
    lcd_update();
    vTaskDelay(pdMS_TO_TICKS(4000));
    device_set_pin(PIN_LCD_EN, 0);

    while (1) {
        lcd_printf(10, 10, 18, COLORED, "work! %d", i++);
        lcd_update();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
