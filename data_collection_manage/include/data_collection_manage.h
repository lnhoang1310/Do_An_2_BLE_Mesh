#ifndef DATA_COLLECTION_MANAGE_H
#define DATA_COLLECTION_MANAGE_H

#include <stdint.h>
#include "dht22.h"
#include "sgp40.h"

typedef struct{
    dht22_t* dht22;
    sgp40_t* sgp40;
}data_t;

void data_collection_manage_init(data_t* data);
void data_collection_manage_update(data_t* data);

#endif