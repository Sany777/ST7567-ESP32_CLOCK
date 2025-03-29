#include "adc_reader.h"

#include <stdio.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_sleep.h"

#define ADC_CHANNEL ADC_CHANNEL_4  // GPIO13 (ESP32)
#define ADC_ATTEN ADC_ATTEN_DB_11    
#define ADC_MAX_VALUE   4095.0F      // 12-bit ADC maximum value
#define VREF            3.3F         // attenuation 11dB
#define DEV_CONST       11.1F        // resistive divider

static adc_oneshot_unit_handle_t adc_handle;

void adc_reader_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&unit_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg);
}

float device_get_voltage(void)
{
    int adc_value = 0;
    adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_value);
    return ((float)adc_value * VREF * DEV_CONST) / ADC_MAX_VALUE;
}