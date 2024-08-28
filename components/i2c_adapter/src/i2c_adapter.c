#include "i2c_adapter.h"

#include "driver/i2c.h"
#include "device_macro.h"
#include "device_common.h"


int I2C_init() 
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    CHECK_AND_RET_ERR(i2c_param_config(I2C_MASTER_NUM, &conf));
    CHECK_AND_RET_ERR(i2c_driver_install(I2C_MASTER_NUM, 
                                        conf.mode, 
                                        0, 
                                        0, 
                                        0));
    return ESP_OK;
}

int I2C_write_reg(uint8_t i2c_addr, uint8_t reg_addr, uint8_t data)
{
    uint8_t send_data[2]; 
    send_data[0] = reg_addr; send_data[1] = data;
    return I2C_write_bytes(i2c_addr, send_data, sizeof(send_data));
}

int I2C_write_byte(uint8_t i2c_addr, uint8_t cmd)
{
    return I2C_write_bytes(i2c_addr, &cmd, sizeof(cmd));
}

int I2C_write_bytes(uint8_t i2c_addr, uint8_t *data, const int length) 
{
    if(length == 0 && data == NULL) return ESP_FAIL;
    i2c_cmd_handle_t  i2c_cmd = i2c_cmd_link_create();
    if (i2c_cmd == NULL) {
        return ESP_ERR_NO_MEM; // Перевірка на помилки при створенні команди
    }
    CHECK_AND_GO(i2c_master_start(i2c_cmd), err_label);
    CHECK_AND_GO(i2c_master_write_byte(i2c_cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true), err_label);
    CHECK_AND_GO(i2c_master_write(i2c_cmd, data, length, true), err_label);
    CHECK_AND_GO(i2c_master_stop(i2c_cmd), err_label);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, i2c_cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(i2c_cmd);
    return ret;

err_label:
    i2c_cmd_link_delete(i2c_cmd);
    return ESP_FAIL;
}

int I2C_read_reg(uint8_t i2c_addr, int reg_addr, uint8_t *data_rd, const int length) 
{
    if (length <= 0 || data_rd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t i2c_cmd = i2c_cmd_link_create();
    if (i2c_cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    CHECK_AND_GO(i2c_master_start(i2c_cmd), err_label);
    if(reg_addr != NO_DATA_REG ){
        CHECK_AND_GO(i2c_master_write_byte(i2c_cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true), err_label);
        uint8_t reg = reg_addr;
        CHECK_AND_GO(i2c_master_write_byte(i2c_cmd, reg, true), err_label);
        CHECK_AND_GO(i2c_master_start(i2c_cmd), err_label);
    }
    CHECK_AND_GO(i2c_master_write_byte(i2c_cmd, (i2c_addr << 1) | I2C_MASTER_READ, true), err_label);
    if (length > 1) {
        CHECK_AND_GO(i2c_master_read(i2c_cmd, data_rd, length - 1, I2C_MASTER_ACK), err_label);
    }
    CHECK_AND_GO(i2c_master_read_byte(i2c_cmd, data_rd + length - 1, I2C_MASTER_NACK), err_label);
    CHECK_AND_GO(i2c_master_stop(i2c_cmd), err_label);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, i2c_cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(i2c_cmd);
    return ret;

err_label:
    i2c_cmd_link_delete(i2c_cmd);
    return ESP_FAIL;
}

int I2C_read_bytes(uint8_t i2c_addr, uint8_t *data_rd, const int length) 
{
    return I2C_read_reg(i2c_addr, NO_DATA_REG, data_rd, length);
}

