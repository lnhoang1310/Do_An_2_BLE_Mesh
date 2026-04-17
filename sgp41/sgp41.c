#include "sgp41.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define TAG "SGP41"

static uint8_t sgp41_cal_crc(uint8_t data[2])
{
    uint8_t crc = 0xFF;
    for (int i = 0; i < 2; i++)
    {
        crc ^= data[i];
        for (uint8_t bit = 8; bit > 0; --bit)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31u;
            else
                crc = (crc << 1);
        }
    }
    return crc;
}

static bool sgp41_send_command(sgp41_t *sgp41, uint8_t *cmd, uint8_t cmd_len, uint8_t *read_buf, size_t read_size)
{
    esp_err_t ret = i2c_master_transmit_receive(sgp41->dev_handle, cmd, cmd_len, read_buf, read_size, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Send command 0x%04X failed", cmd);
        return false;
    }
    return true;
}

static bool sgp41_execute_conditioning(sgp41_t *sgp41)
{
    uint8_t cmd[8];
    cmd[0] = (SGP41_EXECUTE_CONDITIONING_CMD >> 8) & 0xFF;
    cmd[1] = SGP41_EXECUTE_CONDITIONING_CMD & 0xFF;
    cmd[2] = 0x80;
    cmd[3] = 0x00;
    cmd[4] = 0xA2;
    cmd[5] = 0x66;
    cmd[6] = 0x66;
    cmd[7] = 0x93;
    for (uint8_t i = 0; i < 9; i++)
    {
        if (!sgp41_send_command(sgp41, cmd, (uint8_t)sizeof(cmd), NULL, 0))
        {
            ESP_LOGE(TAG, "Execute conditioning failed");
            sgp41->status = SGP41_ERROR;
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return true;
}

static bool sgp41_self_test(sgp41_t *sgp41)
{
    uint8_t cmd[2] = {(SGP41_EXECUTE_SELF_TEST_CMD >> 8) & 0xFF, SGP41_EXECUTE_SELF_TEST_CMD & 0xFF};
    uint8_t response[3];
    if (!sgp41_send_command(sgp41, cmd, (uint8_t)sizeof(cmd), response, sizeof(response)))
    {
        sgp41->status = SGP41_ERROR;
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(320));
    if (sgp41_cal_crc(response) != response[2])
    {
        ESP_LOGE(TAG, "CRC check self test failed");
        sgp41->status = SGP41_ERROR;
        return false;
    }

    if (response[0] != 0xD4)
    {
        ESP_LOGE(TAG, "Self test failed, expected 0xD4, got 0x%02X", response[0]);
        sgp41->status = SGP41_ERROR;
        return false;
    }
    return true;
}

static bool sgp41_get_serial_number(sgp41_t *sgp41)
{
    uint8_t cmd[2] = {(SGP41_GET_SERIAL_NUMBER_CMD >> 8) & 0xFF, SGP41_GET_SERIAL_NUMBER_CMD & 0xFF};
    uint8_t response[9];
    if (!sgp41_send_command(sgp41, cmd, (uint8_t)sizeof(cmd), response, sizeof(response)))
    {
        sgp41->status = SGP41_ERROR;
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
    if (sgp41_cal_crc(&response[0]) != response[2] || sgp41_cal_crc(&response[3]) != response[5] || sgp41_cal_crc(&response[6]) != response[8])
    {
        ESP_LOGE(TAG, "CRC check get serial number failed");
        sgp41->status = SGP41_ERROR;
        return false;
    }
    uint32_t serial_number = ((uint32_t)response[0] << 24) | ((uint32_t)response[1] << 16) | ((uint32_t)response[3] << 8) | response[4];
    ESP_LOGI(TAG, "SGP41 Serial Number: 0x%08X", serial_number);
    return true;
}

static bool sgp41_turn_heater_off(sgp41_t *sgp41)
{
    uint8_t cmd[2] = {(SGP41_TURN_HEATER_OFF_CMD >> 8) & 0xFF, SGP41_TURN_HEATER_OFF_CMD & 0xFF};
    if (!sgp41_send_command(sgp41, cmd, (uint8_t)sizeof(cmd), NULL, 0))
    {
        ESP_LOGE(TAG, "Turn heater off failed");
        sgp41->status = SGP41_ERROR;
        return false;
    }
    return true;
}

void sgp41_init(sgp41_t *sgp41, i2c_port_t i2c_num, gpio_num_t sda_pin, gpio_num_t scl_pin)
{
    if (sgp41 == NULL)
        return;

    sgp41->i2c_port = i2c_num;
    sgp41->sda_pin = sda_pin;
    sgp41->scl_pin = scl_pin;

    esp_err_t ret;

    i2c_master_bus_config_t bus_config = {
        .i2c_port = sgp41->i2c_port,
        .sda_io_num = sgp41->sda_pin,
        .scl_io_num = sgp41->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
    };

    ret = i2c_new_master_bus(&bus_config, &sgp41->bus_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Create I2C bus failed");
        sgp41->status = 1;
        return;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SGP41_ADDRESS,
        .scl_speed_hz = SGP41_I2C_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(sgp41->bus_handle, &dev_config, &sgp41->dev_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Add SGP41 device failed");
        sgp41->status = 1;
        return;
    }

    if (!sgp41_self_test(sgp41))
    {
        ESP_LOGE(TAG, "SGP41 self test init failed");
        sgp41->status = SGP41_ERROR;
        return;
    }

    if (!sgp41_execute_conditioning(sgp41))
    {
        ESP_LOGE(TAG, "SGP41 execute conditioning init failed");
        sgp41->status = SGP41_ERROR;
        return;
    }
}

void sgp41_update(sgp41_t *sgp41, float humidity, float temperature)
{
    if (sgp41 == NULL)
        return;

    uint16_t raw_humidity = (uint16_t)((humidity * 65535) / 100);
    uint16_t raw_temperature = (uint16_t)(((temperature + 45) * 65535) / 175);

    uint8_t cmd[8];
    cmd[0] = (SGP41_MEASURE_RAW_SIGNAL_CMD >> 8) & 0xFF;
    cmd[1] = SGP41_MEASURE_RAW_SIGNAL_CMD & 0xFF;

    cmd[2] = (raw_humidity >> 8) & 0xFF;
    cmd[3] = raw_humidity & 0xFF;
    cmd[4] = sgp41_cal_crc(&cmd[2]);

    cmd[5] = (raw_temperature >> 8) & 0xFF;
    cmd[6] = raw_temperature & 0xFF;
    cmd[7] = sgp41_cal_crc(&cmd[5]);

    uint8_t response[6];

    if (!sgp41_send_command(sgp41, cmd, (uint8_t)sizeof(cmd), response, 3))
    {
        sgp41->status = SGP41_ERROR;
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    if (sgp41_cal_crc(response) != response[2] || sgp41_cal_crc(&response[3]) != response[5])
    {
        ESP_LOGE(TAG, "CRC check failed");
        sgp41->status = SGP41_ERROR;
        return;
    }

    uint16_t raw_voc = (response[0] << 8) | response[1];
    uint16_t raw_nox = (response[3] << 8) | response[4];

    sgp41->voc = raw_voc;
    sgp41->nox = raw_nox;
    sgp41->status = SGP41_OK;
}
