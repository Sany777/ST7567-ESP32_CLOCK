#ifndef ADC_READER_H
#define ADC_READER_H

#ifdef __cplusplus
extern "C" {
#endif

#define MESUR_NUM 1



void voltage_reader_init(void);
float device_get_voltage(void);



#ifdef __cplusplus
}
#endif



#endif // ADC_READER_H
