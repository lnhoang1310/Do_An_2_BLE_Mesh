#ifndef DHT22_H
#define DHT22_H

#include <stdint.h>
#include "driver/gpio.h"

#define DHT22_PIN GPIO_NUM_4

typedef enum{
    DHT22_OK = 0,
    DHT22_ERROR
}dht22_status_t;

typedef struct{
    gpio_num_t pin;
    float temperature;
    float humidity;
    dht22_status_t status;
}dht22_t;

void dht22_init(dht22_t* dht);
void dht22_update(dht22_t* dht);

#endif