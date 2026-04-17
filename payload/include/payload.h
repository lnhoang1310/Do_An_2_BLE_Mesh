#ifndef PAY_LOAD_H
#define PAY_LOAD_H

#include <stdint.h>
#include "data_collection_manage.h"

#define PAYLOAD_NODE_ID 0x00
#define PAYLOAD_TYPE_DATA 0x01

#define PAYLOAD_STATUS_OK 0x00
#define PAYLOAD_STATUS_SENSOR_DHT22_ERROR_POSITION 0
#define PAYLOAD_STATUS_SENSOR_PM25_ERROR_POSITION 1
#define PAYLOAD_STATUS_SENSOR_SGP40_ERROR_POSITION 2

typedef struct{
    int16_t temperature;
    uint16_t humidity;
}payload_data_convert_t;

typedef struct{
    uint8_t type;
    uint8_t node_id;
    uint8_t length;
    union{
        payload_data_convert_t data;
        uint8_t status;
    };
}__attribute__((packed)) mesh_payload_t;

void payload_build(mesh_payload_t* payload, uint8_t type, data_t* data);

#endif