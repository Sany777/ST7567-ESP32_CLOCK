#ifndef __LCD_H
#define __LCD_H

#include "fontlibrary.h"
#include <stdbool.h>
#include "stdarg.h"


/* ------------------------------- SETTINGS -------------------------------- */
// Include relevant ST HAL
// Display parameters

#define PIN_NUM_MOSI    16
#define PIN_NUM_CLK     17
#define PIN_NUM_CS      5
#define PIN_NUM_DC      18
#define PIN_NUM_RST     23


#define LCD_WIDTH			128
#define LCD_HEIGHT			64
#define LCD_PAGES			8
#define LCD_BUFFER_SIZE   	(LCD_WIDTH * LCD_HEIGHT / LCD_PAGES)


#define LCD_TIMEOUT  				100


typedef enum {
    COLORED,
    UNCOLORED
}color_t;

typedef enum {
    HORISONTAL,
    VERTICAL
}direction_t;

typedef enum{
	FONT_SIZE_9,
	FONT_SIZE_18,
}font_size_t;

/* ------------------------------- FUNCTIONS ------------------------------- */
void lcd_init(void);
void lcd_fill(color_t color);


void lcd_draw_pixel(uint8_t x, uint8_t y, color_t color);


void lcd_set_cursor(uint8_t x, uint8_t y);

void lcd_set_contrast(uint8_t val);
void lcd_power_save(bool enable);

void lcd_reset(void);
void lcd_draw_rectangle(int x, int y, int width, int height,  color_t color) ;
void lcd_draw_circle(int x0, int y0, int radius, color_t color) ;
void lcd_update();

void lcd_draw_line(uint8_t hor, int ver, int len, color_t color, direction_t horisontal, int gap);
void lcd_draw_house(int h, int v, int width, int height, color_t color);
void lcd_print_centered_str(uint8_t ver, font_size_t font_size, color_t color, const char *str);
void lcd_print_str(uint8_t hor, uint8_t ver, font_size_t font_size, color_t color, const char *str) ;
void lcd_printf(int hor, int ver, font_size_t font_size, color_t colored, const char *format, ...);
void lcd_printf_centered(int ver, font_size_t font_size, color_t colored, const char *format, ...);




#endif /* __LCD_H */

