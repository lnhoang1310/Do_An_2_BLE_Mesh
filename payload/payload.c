#include <stdio.h>
#include "payload.h"

static payload_data_convert_t data_convert;

static void data_convert_update(data_t* data)
{
    data_convert.temperature = (int16_t)(data->dht22->temperature * 10);
    data_convert.humidity = (uint16_t)(data->dht22->humidity * 10);
}

void payload_build(mesh_payload_t* payload, uint8_t type, data_t* data)
{
    payload->node_id = PAYLOAD_NODE_ID;
    payload->type = type;
    if(type == PAYLOAD_TYPE_DATA){
        data_convert_update(data);
        payload->length = sizeof(payload_data_convert_t);
        payload->data = data_convert;
    }else{
        uint8_t status = PAYLOAD_STATUS_OK;
        if(data->dht22->status != DHT22_OK) status |= (1 << PAYLOAD_STATUS_SENSOR_DHT22_ERROR_POSITION);
        if(data->sgp41->status != SGP41_OK) status |= (1 << PAYLOAD_STATUS_SENSOR_SGP41_ERROR_POSITION);
        payload->status = status;
        payload->length = sizeof(uint8_t);
    }
}
