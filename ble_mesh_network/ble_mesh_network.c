#include "ble_mesh_network.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"


#define TAG "MESH_NODE"
#define CID_ESP 0x02E5

#define VND_MODEL_ID_SERVER 0x0001

#define VND_OP_REQUEST ESP_BLE_MESH_MODEL_OP_3(0x00, CID_ESP)
#define VND_OP_DATA ESP_BLE_MESH_MODEL_OP_3(0x01, CID_ESP)

static uint8_t dev_uuid[16] = {0x32, 0x10};
static uint8_t is_provisioned = 0;
static uint16_t node_addr = 0;
static uint16_t g_net_idx;
static uint16_t g_app_idx;
static data_t* data = NULL;

static esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
};

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
};

static esp_ble_mesh_model_op_t vnd_op[] = {
    ESP_BLE_MESH_MODEL_OP(VND_OP_REQUEST, 0),
    ESP_BLE_MESH_MODEL_OP_END,
};

static esp_ble_mesh_model_t vnd_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, VND_MODEL_ID_SERVER, vnd_op, NULL, NULL),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = ARRAY_SIZE(elements),
    .elements = elements,
};

static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid,
};

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    node_addr = addr;
    is_provisioned = 1;
    ESP_LOGI(TAG, "Provisioned: addr = 0x%04x", addr);
}

static void prov_cb(esp_ble_mesh_prov_cb_event_t event, esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event)
    {
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        prov_complete(param->node_prov_complete.net_idx,
                      param->node_prov_complete.addr,
                      param->node_prov_complete.flags,
                      param->node_prov_complete.iv_index);
        break;

    default:
        break;
    }
}

static void config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event, esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT)
    {
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD)
        {
            g_net_idx = param->value.state_change.appkey_add.net_idx;
            g_app_idx = param->value.state_change.appkey_add.app_idx;
            ESP_LOGI(TAG, "AppKey bind OK: net_idx=0x%04x app_idx=0x%04x", g_net_idx, g_app_idx);
        }
    }
}

static void model_cb(esp_ble_mesh_model_cb_event_t event, esp_ble_mesh_model_cb_param_t *param)
{
    switch (event)
    {
    case ESP_BLE_MESH_MODEL_OPERATION_EVT:
        if (param->model_operation.opcode == VND_OP_REQUEST)
        {
            ESP_LOGI(TAG, "Received REQUEST");
            mesh_payload_t payload;
            payload_build(&payload, PAYLOAD_TYPE_DATA, data);

            esp_err_t err = esp_ble_mesh_server_model_send_msg(&vnd_models[0], param->model_operation.ctx, VND_OP_DATA, sizeof(payload), (uint8_t *)&payload);
            if (err != ESP_OK)
                ESP_LOGE(TAG, "Send DATA failed");
            else
                ESP_LOGI(TAG, "Send DATA OK");
        }
        break;

    default:
        break;
    }
}

void ble_mesh_get_dev_uuid(uint8_t *dev_uuid)
{
    memcpy(dev_uuid + 2, esp_bt_dev_get_address(), BD_ADDR_LEN);
}

esp_err_t bluetooth_init(void)
{
    esp_err_t ret;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
        return ret;

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
        return ret;

    ret = esp_bluedroid_init();
    if (ret)
        return ret;

    ret = esp_bluedroid_enable();
    return ret;
}

static esp_err_t ble_mesh_init(void)
{
    esp_ble_mesh_register_prov_callback(prov_cb);
    esp_ble_mesh_register_config_server_callback(config_server_cb);
    esp_ble_mesh_register_custom_model_callback(model_cb);

    ESP_ERROR_CHECK(esp_ble_mesh_init(&provision, &composition));
    ESP_ERROR_CHECK(esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
    ESP_LOGI(TAG, "BLE Mesh Node initialized");
    return ESP_OK;
}

void ble_mesh_network_init(data_t *data_ptr)
{
    data = data_ptr;
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(bluetooth_init());
    ble_mesh_get_dev_uuid(dev_uuid);
    ESP_ERROR_CHECK(ble_mesh_init());
    esp_ble_mesh_set_unprovisioned_device_name("SENSOR_NODE");
    ESP_LOGI(TAG, "BLE Mesh Node initialized with name: SENSOR_NODE");
}

uint16_t ble_mesh_network_get_addr(void)
{
    return node_addr;
}

uint8_t ble_mesh_network_is_provisioned(void)
{
    return is_provisioned;
}