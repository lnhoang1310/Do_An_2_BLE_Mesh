#include <stdio.h>
#include "data_collection_manage.h"
#include "esp_log.h"

#define TAG "DATA_MANAGE"

static dht22_t dht;
static sgp41_t sgp;
static pms5003_t pms;

void data_collection_manage_init(data_t *data)
{
    if (data == NULL)
        return;

    dht22_init(&dht);
    sgp41_init(&sgp, I2C_NUM_0, GPIO_NUM_22, GPIO_NUM_21);
    pms5003_init(&pms);
    if (dht.status != DHT22_OK)
        ESP_LOGE(TAG, "DHT22 initialization failed");

    if (sgp.status != SGP41_OK)
        ESP_LOGE(TAG, "SGP41 initialization failed");

    if (pms.status != PMS5003_OK)
        ESP_LOGE(TAG, "PMS5003 initialization failed");
    data->dht22 = &dht;
    data->sgp41 = &sgp;
    data->pms5003 = &pms;
}

void data_collection_manage_update(data_t *data)
{
    if (data == NULL)
        return;

    dht22_update(data->dht22);
    if (data->dht22->status != DHT22_OK)
    {
        ESP_LOGE(TAG, "DHT22 update failed");
        return;
    }

    sgp41_update(data->sgp41, data->dht22->humidity, data->dht22->temperature);
    if (data->sgp41->status != SGP41_OK)
    {
        ESP_LOGE(TAG, "SGP41 update failed");
        return;
    }

    pms5003_update(data->pms5003);
    if (data->pms5003->status != PMS5003_OK)
    {
        ESP_LOGE(TAG, "PMS5003 update failed");
        return;
    }
}
