#ifndef BLE_MESH_NETWORK_H
#define BLE_MESH_NETWORK_H

#include <stdint.h>
#include "payload.h"

void ble_mesh_network_init(data_t* data);
uint16_t ble_mesh_network_get_addr(void);
uint8_t ble_mesh_network_is_provisioned(void);

#endif