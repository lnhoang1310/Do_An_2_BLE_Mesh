#ifndef DATA_COLLECTION_MANAGE_H
#define DATA_COLLECTION_MANAGE_H

#include <stdint.h>
#include "dht22.h"
#include "sgp41.h"
#include "pms5003.h"

typedef struct{
    dht22_t* dht22;
    sgp41_t* sgp41;
    pms5003_t* pms5003;
}data_t;

void data_collection_manage_init(data_t* data);
void data_collection_manage_update(data_t* data);

#endif