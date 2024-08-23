#ifndef I2C_MODULE_H
#define I2C_MODULE_H



#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"


#define I2C_MASTER_NUM              I2C_NUM_0 // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ          400000    // I2C master clock frequency
#define I2C_MASTER_TIMEOUT_MS       1000

#define NO_DATA_REG 0xffff


int I2C_init();
int I2C_read_reg(uint8_t i2c_addr, int reg_addr, uint8_t *data_rd, const int length);
int I2C_write_byte(uint8_t i2c_addr, uint8_t cmd);
int I2C_write_reg(uint8_t i2c_addr, uint8_t reg_addr, uint8_t data);
int I2C_write_bytes(uint8_t i2c_addr, uint8_t *data, const int length);
int I2C_read_bytes(uint8_t i2c_addr, uint8_t *data_rd, const int length);
int I2C_read_response(uint8_t i2c_addr, uint8_t *data, const int length);


#ifdef __cplusplus
}
#endif



#endif



