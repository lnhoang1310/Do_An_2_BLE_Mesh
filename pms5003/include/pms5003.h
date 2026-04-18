#ifndef PMS5003_H
#define PMS5003_H

#include <stdint.h>
#include <driver/uart.h>
#include <driver/gpio.h>

#define PMS5003_UART_NUM UART_NUM_1
#define PMS5003_UART_TX_PIN GPIO_NUM_17
#define PMS5003_UART_RX_PIN GPIO_NUM_16

#define PMS5003_START_CMD 0x424D
#define PMS5003_FRAME_LENGTH 32
#define PMS5003_CHANGE_MODE_CMD 0xE1
#define PMS5003_SLEEP_SET_CMD 0xE4
#define PMS5003_READ_PASSIVE_CMD 0xE2

#define PMS5003_START_FRAME 0x424D

#define PMS5003_MODE_ACTIVE_CMD 0x01
#define PMS5003_MODE_PASSIVE_CMD 0x00
#define PMS5003_SLEEP_CMD 0x00
#define PMS5003_WAKE_CMD 0x01

#define PMS5003_READ_TIMEOUT_MS 3000

typedef enum
{
    PMS5003_OK = 0,
    PMS5003_ERROR
} pms5003_status_t;

typedef enum{
    PMS5003_MODE_ACTIVE = 0,
    PMS5003_MODE_PASSIVE
}pms5003_mode_t;

typedef struct
{
    uart_port_t uart_port;
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
    uint16_t pm1_0;
    uint16_t pm2_5;
    uint16_t pm10;
    pms5003_status_t status;
    pms5003_mode_t mode;
} pms5003_t;

void pms5003_init(pms5003_t *pms);
void pms5003_update(pms5003_t *pms);

#endif