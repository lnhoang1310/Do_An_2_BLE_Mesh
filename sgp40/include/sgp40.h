#ifndef SGP40_H
#define SGP40_H

#include <stdint.h>
#include "driver/i2c_master.h"

#define SGP40_ADDRESS 0x59
#define SGP40_I2C_PORT I2C_NUM_0
#define SGP40_SCL_PIN GPIO_NUM_22
#define SGP40_SDA_PIN GPIO_NUM_21
#define SGP40_I2C_FREQ_HZ 100000

#define SGP40_MEASURE_RAW_SIGNAL_CMD 0x260F
#define SGP40_EXECUTE_SELF_TEST_CMD 0x280E
#define SGP40_TURN_HEATER_OFF_CMD 0x3615
#define SGP40_GET_SERIAL_NUMBER_CMD 0x3682

typedef enum
{
    SGP40_OK = 0,
    SGP40_ERROR
} sgp40_status_t;

typedef struct
{
    i2c_port_t i2c_port;
    gpio_num_t scl_pin;
    gpio_num_t sda_pin;

    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;

    uint16_t data;
    sgp40_status_t status;
} sgp40_t;

void sgp40_init(sgp40_t *sgp40, i2c_port_t i2c_port, gpio_num_t scl_pin, gpio_num_t sda_pin);
void sgp40_update(sgp40_t *sgp40, float humidity, float temperature);

#endif