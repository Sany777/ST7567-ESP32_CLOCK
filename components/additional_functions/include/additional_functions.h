#ifndef ADD_FUNCTIONS_H
#define ADD_FUNCTIONS_H



#ifdef __cplusplus
extern "C" {
#endif

// class AutoLock{
//     // SemaphoreHandle_t _semaphore;
// public:
//     AutoLock(int &semaphore);
//     ~AutoLock();
// };

// #define ENTER_CRITICAL(semaphore)    
//     AutoLock _al(semaphore)





// #include "cstdint"



unsigned get_num(char *data, unsigned size);
char * num_to_str(char *buf, unsigned num, unsigned char digits, const unsigned char base);
unsigned num_arr_to_str(char *dst, unsigned *src, unsigned char dst_digits, unsigned src_size);




#ifdef __cplusplus
}
#endif


#endif



