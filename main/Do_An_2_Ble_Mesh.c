#include <stdio.h>
#include "ble_mesh_network.h"
#include "data_collection_manage.h"
#include "freertos/FreeRTOS.h"

data_t data;

void sensor_task(void* arg)
{
    while(1){
        data_collection_manage_update(&data);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Cập nhật dữ liệu mỗi 5 giây
    }
}

void app_main(void)
{
    data_collection_manage_init(&data);
    ble_mesh_network_init(&data);
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}
