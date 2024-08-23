#ifndef ADC_READER_H
#define ADC_READER_H

#ifdef __cplusplus
extern "C" {
#endif

#define MESUR_NUM 1

void adc_reader_init(void);
float adc_reader_get_voltage(void);

#ifdef __cplusplus
}
#endif



#endif // ADC_READER_H
