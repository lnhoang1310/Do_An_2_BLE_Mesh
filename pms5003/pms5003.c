#include <stdio.h>
#include "pms5003.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_err.h"

#define TAG "PMS5003"

#define UART_BUFFER_SIZE 256

static QueueHandle_t uart_queue;
uint8_t frame[PMS5003_FRAME_LENGTH + 4];
static uint8_t frame_index = 0;
static TickType_t last_rx_time = 0;

static uint16_t cal_verify_byte(uint8_t *data, uint8_t length)
{
    uint16_t sum = 0;
    for (uint8_t i = 0; i < length; i++)
        sum += data[i];
    return sum;
}

static void pms5003_uart_task(void *pvParameters)
{
    pms5003_t *pms = (pms5003_t *)pvParameters;
    uart_event_t event;
    uint8_t data[128];

    while (1)
    {
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY))
        {
            if (event.type == UART_DATA)
            {
                int len = uart_read_bytes(pms->uart_port, data, event.size, 0);

                if (len > 0)
                {
                    for (uint16_t i = 0; i < len; i++)
                    {
                        uint8_t byte = data[i];
                        if (frame_index == 0 && byte != ((PMS5003_START_FRAME >> 8) & 0xFF))
                            continue;
                        if (frame_index == 1)
                        {
                            if (byte != (PMS5003_START_FRAME & 0xFF))
                            {
                                frame_index = 0;
                                continue;
                            }
                        }
                        frame[frame_index++] = byte;

                        // full frame
                        if (frame_index >= PMS5003_FRAME_LENGTH)
                        {
                            uint16_t verify = cal_verify_byte(frame, 30);
                            if (verify != ((frame[30] << 8) | frame[31]))
                            {
                                ESP_LOGE(TAG, "Frame verify failed");
                                pms->status = PMS5003_ERROR;
                            }
                            else
                            {
                                pms->pm1_0 = (frame[4] << 8) | frame[5];
                                pms->pm2_5 = (frame[6] << 8) | frame[7];
                                pms->pm10 = (frame[8] << 8) | frame[9];
                                pms->status = PMS5003_OK;
                                last_rx_time = xTaskGetTickCount();
                            }
                            frame_index = 0;
                        }
                    }
                }
            }
            else if (event.type == UART_FIFO_OVF)
            {
                ESP_LOGE(TAG, "UART FIFO overflow");
                uart_flush_input(pms->uart_port);
                xQueueReset(uart_queue);
            }
        }
    }
}

static void pms5003_change_mode(pms5003_t *pms, pms5003_mode_t mode)
{
    uint8_t cmd[7];
    cmd[0] = (PMS5003_CHANGE_MODE_CMD >> 8) & 0xFF;
    cmd[1] = PMS5003_CHANGE_MODE_CMD & 0xFF;
    cmd[2] = PMS5003_CHANGE_MODE_CMD;
    cmd[3] = 0x00;
    cmd[4] = (mode == PMS5003_MODE_ACTIVE) ? PMS5003_MODE_ACTIVE_CMD : PMS5003_MODE_PASSIVE_CMD;

    uint16_t verify = cal_verify_byte(&cmd[0], 5);
    cmd[5] = (verify >> 8) & 0xFF;
    cmd[6] = verify & 0xFF;
    if (uart_write_bytes(PMS5003_UART_NUM, (const char *)cmd, (uint8_t)sizeof(cmd)) != (uint8_t)sizeof(cmd))
    {
        ESP_LOGE(TAG, "UART write failed");
        pms->status = PMS5003_ERROR;
        return;
    }
    pms->mode = mode;
}

void pms5003_init(pms5003_t *pms)
{
    esp_err_t ret;
    pms->uart_port = PMS5003_UART_NUM;
    pms->tx_pin = PMS5003_UART_TX_PIN;
    pms->rx_pin = PMS5003_UART_RX_PIN;
    pms->pm1_0 = 0;
    pms->pm2_5 = 0;
    pms->pm10 = 0;
    pms->status = PMS5003_OK;
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    ret = uart_param_config(pms->uart_port, &uart_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "UART param config failed");
        pms->status = PMS5003_ERROR;
        return;
    }
    ret = uart_set_pin(pms->uart_port, pms->tx_pin, pms->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "UART set pin failed");
        pms->status = PMS5003_ERROR;
        return;
    }
    ret = uart_driver_install(pms->uart_port, UART_BUFFER_SIZE * 2, UART_BUFFER_SIZE * 2, 20, &uart_queue, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "UART driver install failed");
        pms->status = PMS5003_ERROR;
        return;
    }
    xTaskCreate(pms5003_uart_task, "pms5003_uart_task", 2048, pms, 10, NULL);
    pms5003_change_mode(pms, PMS5003_MODE_ACTIVE);
}

void pms5003_update(pms5003_t *pms)
{
    if (pms->mode == PMS5003_MODE_PASSIVE)
    {
        uint8_t cmd[7];
        cmd[0] = (PMS5003_START_CMD >> 8) & 0xFF;
        cmd[1] = PMS5003_START_CMD & 0xFF;
        cmd[2] = PMS5003_READ_PASSIVE_CMD;
        cmd[3] = 0x00;
        cmd[4] = 0x00;
        uint16_t verify = cal_verify_byte(&cmd[0], 4);
        cmd[5] = (verify >> 8) & 0xFF;
        cmd[6] = verify & 0xFF;
        if (uart_write_bytes(PMS5003_UART_NUM, (const char *)cmd, (uint8_t)sizeof(cmd)) != (uint8_t)sizeof(cmd))
        {
            ESP_LOGE(TAG, "UART write failed");
            pms->status = PMS5003_ERROR;
            return;
        }
    }
    else
    {
        if (xTaskGetTickCount() - last_rx_time > pdMS_TO_TICKS(PMS5003_READ_TIMEOUT_MS))
        {
            ESP_LOGE(TAG, "Read timeout");
            pms->status = PMS5003_ERROR;
        }
    }
}
