#include "lcd.h"
#include <stdint.h>
#include <stdbool.h>

#include "string.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "portmacro.h"

#include "device_common.h"

#define LCD_DISPLAY_ON 					0xAF
#define LCD_DISPLAY_OFF					0xAE
#define LCD_SET_START_LINE				0x40			// + line0 - line63
#define LCD_SEG_NORMAL					0xA0
#define LCD_SEG_REVERSE					0xA1
#define LCD_COLOR_NORMAL				0xA6
#define LCD_COLOR_INVERSE				0xA7
#define LCD_DISPLAY_DRAM				0xA4
#define LCD_DISPLAY_ALL_ON				0xA5
#define LCD_SW_RESET					0xE2
#define LCD_COM_NORMAL					0xC0
#define LCD_COM_REVERSE					0xC8
#define LCD_POWER_CONTROL				0x28
#define LCD_SET_RR							0x20			// + RR[2:0]; 3.0, 3.5, ..., 6.5
#define LCD_SET_EV_CMD					0x81
#define LCD_NOP									0xE3

#define LCD_PAGE_ADDR						0xB0			// + 0x0 - 0x7 -> page0 - page7
#define LCD_COL_ADDR_H					0x10			// + X[7:4]
#define LCD_COL_ADDR_L					0x00			// + X[3:0]

#define LCD_BIAS7								0xA3
#define LCD_BIAS9								0xA2

#define LCD_PWR_BOOSTER_ON			0x04
#define LCD_PWR_REGULATOR_ON		0x02
#define LCD_PWR_FOLLOWER_ON			0x01

typedef struct {
		uint8_t curr_x;
		uint8_t curr_y;
} lcd_pos_t;

#define LCD_WIDTH 128
#define LCD_HEIGHT 64

static uint8_t screen_buf[LCD_BUFFER_SIZE];
static lcd_pos_t lcd;
static spi_device_handle_t spi;
static char text_buf[150];


static void lcd_write_char(char ch, fontStyle_t *font, color_t color);


static void lcd_send_cmd(const uint8_t cmd) 
{
    gpio_set_level(PIN_NUM_DC, 0); 
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
        .user = NULL,
    };
    spi_device_transmit(spi, &t);
}

static void lcd_send_data(uint8_t data)
{
    gpio_set_level(PIN_NUM_DC, 1); 
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
        .user = NULL,
    };
    spi_device_transmit(spi, &t);
}


void lcd_reset(void) 
{
	gpio_set_level(PIN_NUM_RST, 0);
	vTaskDelay(pdMS_TO_TICKS(200));
	gpio_set_level(PIN_NUM_RST, 1);
	vTaskDelay(pdMS_TO_TICKS(1000));
}


void lcd_init(void) 
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10000000,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
    };

    spi_bus_initialize(SPI2_HOST, &buscfg, 1);
    spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
	
	gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
	gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
	gpio_set_direction(PIN_LCD_BACKLIGHT_EN, GPIO_MODE_OUTPUT);
	lcd_reset();

	lcd_send_cmd(LCD_BIAS7);
	lcd_send_cmd(LCD_SEG_NORMAL);
	lcd_send_cmd(LCD_COM_REVERSE);
	lcd_send_cmd(LCD_SET_RR | 0x4);		// regulation ratio 5.0

	lcd_send_cmd(LCD_SET_EV_CMD);
	lcd_send_cmd(0);	

	lcd_send_cmd(LCD_POWER_CONTROL | LCD_PWR_BOOSTER_ON);
	lcd_send_cmd(LCD_POWER_CONTROL | LCD_PWR_BOOSTER_ON | LCD_PWR_REGULATOR_ON);
	lcd_send_cmd(LCD_POWER_CONTROL | LCD_PWR_BOOSTER_ON | LCD_PWR_REGULATOR_ON | LCD_PWR_FOLLOWER_ON);

	lcd_fill(UNCOLORED);
	lcd_update();
	lcd_send_cmd(LCD_DISPLAY_DRAM);
	lcd_send_cmd(LCD_DISPLAY_ON);
}


void lcd_fill(color_t color) 
{
	uint8_t val = (color == COLORED) ? 0xFF : 0x00;

	for(uint16_t i = 0; i < LCD_BUFFER_SIZE; ++i) {
		screen_buf[i] = val;
	}
}


void lcd_power_save(bool enable) {
	if(enable) {
		lcd_send_cmd(LCD_DISPLAY_OFF);
		lcd_send_cmd(LCD_DISPLAY_ALL_ON);
	}
	else {
		lcd_send_cmd(LCD_DISPLAY_DRAM);
		lcd_send_cmd(LCD_DISPLAY_ON);
	}
}

void lcd_draw_pixel(uint8_t x, uint8_t y, color_t color) 
{
	if(x >= LCD_WIDTH || y >= LCD_HEIGHT) {
		return;
	}

	// Draw in the right color
	if(color == COLORED) {
		screen_buf[x + (y / 8) * LCD_WIDTH] |= 1 << (y % 8);
	} else {
		screen_buf[x + (y / 8) * LCD_WIDTH] &= ~(1 << (y % 8));
	}
}

void lcd_draw_ver_line(uint8_t hor, int ver, int height, color_t color)  
{
	const int botom = ver+height;
	for(int x = hor, y = ver; y<LCD_HEIGHT && y<botom; ++y) {
		lcd_draw_pixel(x, y, color);
	}
}

void lcd_draw_hor_line(uint8_t hor, int ver, int width, color_t color) 
{
	const int right = width + hor;
	for(int x = hor, y = ver; x < LCD_WIDTH && x<right; ++x) {
		lcd_draw_pixel(x, y, color);
	}
}

void lcd_set_cursor(uint8_t x, uint8_t y) 
{
	lcd.curr_x = x;
	lcd.curr_y = y;
}

static void lcd_write_char(char ch, fontStyle_t *font, color_t color) 
{
	if(ch < font->FirstAsciiCode) {
		ch = 0;
	}
	else {
		ch -= font->FirstAsciiCode;
	}
	if (LCD_WIDTH < (lcd.curr_x + font->GlyphWidth[(int)ch]) ||
		LCD_HEIGHT < (lcd.curr_y + font->GlyphWidth[(int)ch])) {
		return;
	}

	uint32_t chr;

	for(uint32_t j = 0; j < font->GlyphHeight; ++j) {
		uint8_t width = font->GlyphWidth[(int)ch];

		for(uint32_t w = 0; w < font->GlyphBytesWidth; ++w) {
			chr = font->GlyphBitmaps[(ch * font->GlyphHeight + j) * font->GlyphBytesWidth + w];

			uint8_t w_range = width;
			if(w_range >= 8) {
				w_range = 8;
				width -= 8;
			}

			for(uint32_t i = 0; i < w_range; ++i) {
				if((chr << i) & 0x80)  {
					lcd_draw_pixel(lcd.curr_x + i + w*8, lcd.curr_y + j, color);
				} else {
					lcd_draw_pixel(lcd.curr_x + i + w*8, lcd.curr_y + j, !color);
				}
			}
		}
	}

	lcd.curr_x += font->GlyphWidth[(int)ch];
}


void lcd_print_centered_str(uint8_t ver, font_size_t font_size, color_t color, char *str)
{
	uint8_t hor;
	fontStyle_t *font = font_size == FONT_SIZE_18 ? &FontStyle_videotype_18 : &FontStyle_RetroVilleNC_9;
	if(font_size == FONT_SIZE_18){
		hor = (LCD_WIDTH - strlen(str)*10) / 2;
	} else {
		hor = (LCD_WIDTH - strlen(str)*6.5) / 2;
	}
	if(hor < 0){
		hor = 0;
	}
	lcd_set_cursor(hor, ver);
	while(*str) {
		lcd_write_char(*str++, font, color);
	}
}


void lcd_print_str(uint8_t hor, uint8_t ver, font_size_t font_size, color_t color,  char *str) 
{
	fontStyle_t *font;
	if(font_size == FONT_SIZE_18){
		font = &FontStyle_videotype_18;
	} else {
		font = &FontStyle_RetroVilleNC_9;
	}
	lcd_set_cursor(hor,ver);
	while(*str) {
		lcd_write_char(*str++, font, color);
	}
}


void lcd_printf(int hor, int ver, font_size_t font_size, color_t colored, const char *format, ...)
{
    va_list args;
    va_start (args, format);
    vsnprintf (text_buf, sizeof(text_buf), format, args);
    va_end (args);
    lcd_print_str(hor, ver, font_size, colored, text_buf);
}

void lcd_printf_centered(int ver, font_size_t font_size, color_t colored, const char *format, ...)
{
    va_list args;
    va_start (args, format);
    vsnprintf (text_buf, sizeof(text_buf), format, args);
    va_end (args);
    lcd_print_centered_str(ver, font_size, colored, text_buf);
}


void lcd_clear_buffer(color_t color) 
{
    memset(screen_buf, color == COLORED ? 0xFF : 0, sizeof(screen_buf));
}


void lcd_update() 
{
    for (uint8_t page = 0; page < (LCD_HEIGHT / LCD_PAGES); page++) {
        lcd_send_cmd(0xB0 | page); 
        lcd_send_cmd(0x00); 
        lcd_send_cmd(0x10); 
        for (uint8_t col = 0; col < LCD_WIDTH; col++) {
            lcd_send_data(screen_buf[page * LCD_WIDTH + col]);
        }
    }
}


void lcd_draw_circle(int x0, int y0, int radius, color_t color) 
{
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        lcd_draw_pixel(x0 + x, y0 + y, color);
        lcd_draw_pixel(x0 + y, y0 + x, color);
        lcd_draw_pixel(x0 - y, y0 + x, color);
        lcd_draw_pixel(x0 - x, y0 + y, color);
        lcd_draw_pixel(x0 - x, y0 - y, color);
        lcd_draw_pixel(x0 - y, y0 - x, color);
        lcd_draw_pixel(x0 + y, y0 - x, color);
        lcd_draw_pixel(x0 + x, y0 - y, color);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }

        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

void lcd_draw_rectangle(int x, int y, int width, int height,  color_t color) 
{
    for (int i = 0; i < width; i++) {
        lcd_draw_pixel(x + i, y, color);
        lcd_draw_pixel(x + i, y + height, color);
    }

    for (int i = 0; i < height; i++) {
        lcd_draw_pixel(x, y + i, color);
        lcd_draw_pixel(x + width, y + i, color);
    }
}

void lcd_draw_house(int h, int v, int width, int height, color_t color)
{
	int top = v - height;
	if(top < 0)top = 0;
	int l = h, r = h+width, y = v;
	for(; y >= top && y > 0 && y > top;  --r, ++l, --y){
		lcd_draw_pixel(r, y, color);
		lcd_draw_pixel(l, y, color);
	}
	lcd_draw_hor_line(l, top, r-l+1, color);
	lcd_draw_hor_line(h, v, width, color);
	lcd_draw_hor_line(h, v+height, width, color);
	lcd_draw_ver_line(h, v, height, color);
	lcd_draw_ver_line(h+width, v, height, color);
}


void lcd_set_contrast(uint8_t val) 
{
	lcd_send_cmd(LCD_SET_EV_CMD);
	lcd_send_cmd((val & 0x3f));
}

