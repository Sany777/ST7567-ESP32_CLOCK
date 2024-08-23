#ifndef device_memory_H_
#define device_memory_H_


#ifdef __cplusplus
extern "C" {
#endif



int read_flash(const char* data_name, unsigned char *buf, unsigned data_size);
int write_flash(const char* data_name, unsigned char *buf, unsigned data_size);


#ifdef __cplusplus
}
#endif

#endif