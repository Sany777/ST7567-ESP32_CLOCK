#include "dht20.h"

#include <stdio.h>
#include "i2c_adapter.h"
#include "device_macro.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"


static uint8_t dht20_crc8(uint8_t *message, uint8_t Num);

#define DHT20_ADDR                  0x38
#define DHT20_CMD_MEASURE           0xAC
#define DHT20_CMD_RESET             0xBA
#define DHT20_CMD_READ              0xE1

#define DHT20_STATUS_BUSY           0x80
#define DHT20_MAX_BUSY_WAIT_MS      700
#define DHT20_CHECK_BUSY_DELAY_MS   40



uint8_t dht20_status(void)
{
    uint8_t rxdata;
    I2C_write_byte(DHT20_I2C_ADDRESS, 0x71);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    I2C_read_bytes(DHT20_I2C_ADDRESS, &rxdata, 1);
    return (rxdata);
}


bool dht20_is_calibrated(void)
{
    uint8_t status_byte = dht20_status();
    if ((status_byte & 0x68) == 0x08) {
        return true;
    }
    return false;
}

int dht20_wait()
{
    int wait_time = 0;
    uint8_t status_byte[1] = { 0 };                  
    do {
        vTaskDelay(pdMS_TO_TICKS(DHT20_CHECK_BUSY_DELAY_MS));
        I2C_read_bytes(DHT20_ADDR, status_byte, sizeof(status_byte));
        if((status_byte[0] >> 7) == 0){
            return ESP_OK;
        }
        wait_time += DHT20_CHECK_BUSY_DELAY_MS;
    } while(wait_time < DHT20_MAX_BUSY_WAIT_MS);
    return ESP_FAIL;
}


int dht20_read_data(float *temperature, float *humidity)
{
    uint8_t txbuf[3] = {0xAC, 0x33, 0x00};             
    uint8_t rxdata[7] = {0};                         
    I2C_write_bytes(DHT20_I2C_ADDRESS, txbuf, sizeof(txbuf));
    vTaskDelay(80 / portTICK_PERIOD_MS);                

    CHECK_AND_RET_ERR(dht20_wait());

    I2C_read_bytes(DHT20_I2C_ADDRESS, rxdata, sizeof(rxdata));              
                            
    if(humidity){
        uint32_t raw_humid = rxdata[1];
        raw_humid <<= 8;
        raw_humid += rxdata[2];
        raw_humid <<= 4;
        raw_humid += rxdata[3] >> 4;
        *humidity = (float)(raw_humid / 1048576.0f) * 100.0f;       
    }
    if(temperature){
        uint32_t raw_temp = (rxdata[3] & 0x0F);
        raw_temp <<= 8;
        raw_temp += rxdata[4];
        raw_temp <<= 8;
        raw_temp += rxdata[5];
        *temperature = (float)(raw_temp / 1048576.0f) * 200.0f - 50.0f;
    }
    return ESP_OK;
}

static uint8_t dht20_crc8(uint8_t *message, uint8_t Num)
{
    static uint8_t i;
    static uint8_t byte;
    uint8_t crc = 0xFF;
    for (byte = 0; byte < Num; byte++) {
        crc ^= (message[byte]);
        for (i = 8; i > 0; --i) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}