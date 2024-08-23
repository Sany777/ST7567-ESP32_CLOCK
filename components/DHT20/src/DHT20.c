#include "DHT20.h"

#include "stdint.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_adapter.h"
#include "device_macro.h"
#include "device_common.h"


#define DHT20_ADDR                  0x38
#define DHT20_CMD_MEASURE           0xAC
#define DHT20_CMD_RESET             0xBA
#define DHT20_CMD_READ              0xE1

#define DHT20_STATUS_BUSY           0x80
#define DHT20_MAX_BUSY_WAIT_MS      1000
#define DHT20_CHECK_BUSY_DELAY_MS   50

static const char *TAG = "DHT20";


int DHT20_read_data(float *temperature, float *humidity) 
{
    uint8_t measure_cmd[] = {DHT20_CMD_MEASURE};
    uint8_t data[6] = {0};
    uint8_t status;
    int wait_time = 0;
    CHECK_AND_RET_ERR(I2C_write_bytes(DHT20_ADDR, measure_cmd, sizeof(measure_cmd)));
    do {
        vTaskDelay(pdMS_TO_TICKS(DHT20_CHECK_BUSY_DELAY_MS));
        CHECK_AND_RET_ERR(I2C_read_response(DHT20_ADDR, &status, sizeof(status)));
        wait_time += DHT20_CHECK_BUSY_DELAY_MS;
        if (wait_time >= DHT20_MAX_BUSY_WAIT_MS) {
            ESP_LOGE(TAG, "Sensor is busy for too long");
            return ESP_FAIL;
        }
    } while (status&DHT20_STATUS_BUSY);
    
    CHECK_AND_RET_ERR(I2C_read_bytes(DHT20_ADDR, data, sizeof(data)));

    uint32_t raw_humidity = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((uint32_t)data[3] >> 4);
    uint32_t raw_temperature = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5];

    if(humidity)
        *humidity = ((float)raw_humidity / 1048576.0) * 100.0;
    if(temperature)
        *temperature = ((float)raw_temperature / 1048576.0) * 200.0 - 50.0;

    return ESP_OK;
}

