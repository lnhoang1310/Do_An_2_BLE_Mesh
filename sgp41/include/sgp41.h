#ifndef SGP41_H
#define SGP41_H

#include <stdint.h>
#include "driver/i2c_master.h"

#define SGP41_ADDRESS 0x59
#define SGP41_I2C_PORT I2C_NUM_0
#define SGP41_SCL_PIN GPIO_NUM_22
#define SGP41_SDA_PIN GPIO_NUM_21
#define SGP41_I2C_FREQ_HZ 100000

#define SGP41_EXECUTE_CONDITIONING_CMD 0x2612
#define SGP41_MEASURE_RAW_SIGNAL_CMD 0x2619
#define SGP41_EXECUTE_SELF_TEST_CMD 0x280E
#define SGP41_TURN_HEATER_OFF_CMD 0x3615
#define SGP41_GET_SERIAL_NUMBER_CMD 0x3682

typedef enum
{
    SGP41_OK = 0,
    SGP41_ERROR
} sgp41_status_t;

typedef struct
{
    i2c_port_t i2c_port;
    gpio_num_t scl_pin;
    gpio_num_t sda_pin;

    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;

    uint16_t voc;
    uint16_t nox;
    sgp41_status_t status;
} sgp41_t;

void sgp41_init(sgp41_t *sgp41, i2c_port_t i2c_port, gpio_num_t scl_pin, gpio_num_t sda_pin);
void sgp41_update(sgp41_t *sgp41, float humidity, float temperature);

#endif