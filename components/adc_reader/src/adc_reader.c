#include "adc_reader.h"

#include <stdio.h>
#include "driver/adc.h"
#include "esp_log.h"
#include "adc_reader.h"
#include "device_common.h"

#include "freertos/FreeRTOS.h"



#define ADC_CHANNEL ADC2_CHANNEL_4  // GPIO13 (ESP32)
#define ADC_ATTEN ADC_ATTEN_DB_0     
#define ADC_MAX_VALUE 4095          // 12-bit ADC maximum value
#define VREF 1100 



static const char *TAG = "ADC_READER";


void adc_reader_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc2_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    ESP_LOGI(TAG, "ADC Initialized");
}


float adc_reader_get_voltage(void)
{
    int adc_value = 0;
    adc2_get_raw(ADC_CHANNEL,ADC_WIDTH_BIT_12, &adc_value);
    float voltage = (((float)adc_value * 10000) / (ADC_MAX_VALUE*1280)) * (VREF / 1000.0);
    return voltage;
}
