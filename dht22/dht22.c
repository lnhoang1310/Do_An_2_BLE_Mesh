#include <stdio.h>
#include "dht22.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

#define TAG "DHT22"

static void delay_ms(uint32_t ms){
    for(uint32_t i=0; i<ms; i++)
        esp_rom_delay_us(1000);
}

static void set_output(gpio_num_t pin)
{
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
}

static void set_input(gpio_num_t pin)
{
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}

static int16_t wait_level(gpio_num_t pin, int level, uint16_t timeout_us)
{
    int16_t count = 0;
    while (gpio_get_level(pin) == level)
    {
        esp_rom_delay_us(1);
        if (++count > timeout_us)
            return -1;
    }
    return count;
}

void dht22_init(dht22_t* dht)
{
    delay_ms(1000);
    dht->pin = DHT22_PIN;
    dht->temperature = 0.0f;
    dht->humidity = 0.0f;
    dht->status = DHT22_OK;

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << dht->pin),
        .pull_down_en = 0,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(dht->pin, 1);
    dht22_update(dht);
}

void dht22_update(dht22_t* dht){
    uint8_t data[5] = {0};

    set_output(dht->pin);
    gpio_set_level(dht->pin, 0);
    esp_rom_delay_us(1000);
    gpio_set_level(dht->pin, 1);
    esp_rom_delay_us(30);
    set_input(dht->pin);
    
    // response signal
    if(wait_level(dht->pin, 1, 80) < 0) goto error;
    if(wait_level(dht->pin, 0, 80) < 0) goto error;
    if(wait_level(dht->pin, 1, 80) < 0) goto error;

    // start frame 
    for(uint8_t i=0; i<40; i++){
        if(wait_level(dht->pin, 0, 50) < 0) goto error;
        int16_t high_time = wait_level(dht->pin, 1, 100);
        if(high_time < 0) goto error;
        data[i/8] <<= 1;
        if(high_time > 50) data[i/8] |= 1;
    }

    if(data[4] != (data[0] + data[1] + data[2] + data[3])) goto error;

    uint16_t raw_humidity = (data[0] << 8) | data[1];
    uint16_t raw_temperature = (data[2] << 8) | data[3];

    if(raw_humidity > 1000 || raw_temperature > 1250){
        ESP_LOGE(TAG, "Invalid data: raw humidity=%u, raw temperature=%u", raw_humidity, raw_temperature);
        goto error;
    }

    dht->humidity = (float)raw_humidity * 0.1f;
    if(raw_temperature & 0x8000){
        raw_temperature &= 0x7FFF;
        dht->temperature = -((float)raw_temperature * 0.1f);
    }else{
        dht->temperature = (float)raw_temperature * 0.1f;
    }
    dht->status = DHT22_OK;
    return;

error:
    dht->status = DHT22_ERROR;
}


