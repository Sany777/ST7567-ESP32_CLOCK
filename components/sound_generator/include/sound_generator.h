#ifndef SOUND_GENERATOR_H_
#define SOUND_GENERATOR_H_

#ifdef __cplusplus
extern "C" {
#endif



void start_single_signale(unsigned delay, unsigned freq);
void start_signale_series(unsigned delay, unsigned count, unsigned freq);
void start_alarm();
void set_loud(unsigned loud);
void sound_off();




#ifdef __cplusplus
}
#endif









#endif