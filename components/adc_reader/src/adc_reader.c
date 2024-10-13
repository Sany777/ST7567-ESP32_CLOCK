#include "adc_reader.h"

#include <stdio.h>
#include "driver/adc.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"


#define ADC_CHANNEL ADC1_CHANNEL_4  // GPIO13 (ESP32)
#define ADC_ATTEN ADC_ATTEN_DB_11    
#define ADC_MAX_VALUE   4095.0F      // 12-bit ADC maximum value
#define VREF            3.3F         // attenuation 11dB
#define DEV_CONST       11.1F        // resistive divider



void adc_reader_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
}


float device_get_voltage(void)
{
    int adc_value = 0;
    for(int i=0; i<MEAS_NUM; ++i)
        adc_value += adc1_get_raw(ADC_CHANNEL);
    adc_value /= MEAS_NUM;

    return ((float)adc_value * VREF * DEV_CONST) / ADC_MAX_VALUE;
}
