#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "uart_sim.h"
#include "esp_system.h"
#include <stdbool.h>

#define TAG "UART_SIM"

QueueHandle_t uart_event_queue; 
QueueHandle_t tx_queue;         
QueueHandle_t mqtt_queue;       
QueueHandle_t http_queue;      
QueueHandle_t at_resp_queue;    

 
static void uart_tx_task(void *pvParameters) {
    char *cmd;
    while (1) {
        if (xQueueReceive(tx_queue, &cmd, portMAX_DELAY)) {
            uart_write_bytes(SIM_UART, cmd, strlen(cmd));
            ESP_LOGI(TAG, ">> %s", cmd);
            free(cmd); 
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void uart_rx_router_task(void *pvParameters) {
    uart_event_t event;
    static char rx_buf[BUFFER_SIZE];
    int total_len = 0;
    memset(rx_buf, 0, BUFFER_SIZE);

    while (1) {
        if (xQueueReceive(uart_event_queue, (void *)&event, portMAX_DELAY)) {
            if (event.type == UART_DATA) {
                int len = uart_read_bytes(SIM_UART, rx_buf + total_len, event.size, portMAX_DELAY);
                total_len += len;
                rx_buf[total_len] = '\0';

                /// Kiem tra ban tin cua HTTP/MQTT
                if (strstr(rx_buf, "+CMQTTRXEND:") != NULL) {
                    char *p = strstr(rx_buf, "+CMQTTRXPAYLOAD: 0,");
                    if (p) {
                        int data_size = atoi(p + strlen("+CMQTTRXPAYLOAD: 0,"));
                        char *data_start = strstr(p, "\r\n");
                        if (data_start) {
                            data_start += 2;
                            char *mqtt_payload = (char *)malloc(data_size + 1);
                            if (mqtt_payload) {
                                memcpy(mqtt_payload, data_start, data_size);
                                mqtt_payload[data_size] = '\0';
                                xQueueSend(mqtt_queue, &mqtt_payload, 0);
                            }
                        }
                    }
                    memset(rx_buf, 0, BUFFER_SIZE);
                    total_len = 0;
                }

                else if (strstr(rx_buf, "+HTTPACTION:") != NULL) {
                    char *action_resp = strstr(rx_buf, "+HTTPACTION:");
                    int method, status, resp_len;
                    if (sscanf(action_resp, "+HTTPACTION: %d,%d,%d", &method, &status, &resp_len) == 3) {
                        xQueueSend(http_queue, &status, 0);
                    }
                    memset(rx_buf, 0, BUFFER_SIZE);
                    total_len = 0;
                }

                else if (strstr(rx_buf, "OK\r\n") != NULL || strstr(rx_buf, "ERROR\r\n") != NULL || strstr(rx_buf, ">") != NULL || strstr(rx_buf, "DOWNLOAD") != NULL) {
                    char *resp_str = strdup(rx_buf); 
                    if (resp_str) {
                        xQueueSend(at_resp_queue, &resp_str, 0);
                    }
                    memset(rx_buf, 0, BUFFER_SIZE);
                    total_len = 0;
                }
            } 
            else {
                uart_flush_input(SIM_UART);
                xQueueReset(uart_event_queue);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static bool sim_send_at(const char *cmd, const char *expected, int timeout_ms) {
    char *buff_reset;
    while (xQueueReceive(at_resp_queue, &buff_reset, 0) == pdPASS) {
        free(buff_reset);
    }
    if (strlen(cmd) > 0) {
        char *cmd_copy = strdup(cmd);
        if (cmd_copy) xQueueSend(tx_queue, &cmd_copy, 0);
    }
    TickType_t start_tick = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS < timeout_ms) {
        char *resp;
        if (xQueueReceive(at_resp_queue, &resp, pdMS_TO_TICKS(100)) == pdPASS) {
            ESP_LOGI(TAG, "<< %s", resp);
            bool is_expected = (strstr(resp, expected) != NULL);
            bool is_error = (strstr(resp, "ERROR") != NULL);
            free(resp);

            if (is_expected) return true;
            if (is_error) return false;
        }
    }
    ESP_LOGE(TAG, "FAIL (Timeout): %s", cmd);
    return false;
}

bool mqtt_sub_start(const char *topic, int timeout) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CMQTTSUB=0,%d,1\r\n", (int)strlen(topic));
    if (!sim_send_at(cmd, ">", timeout)) return false;
    
    vTaskDelay(pdMS_TO_TICKS(200));
    uart_write_bytes(SIM_UART, topic, strlen(topic));

    return true;
}

bool http_confirm_start_charging(const char *data) {
    char cmd_url[256];
    char cmd_data[64];
    int body_len = strlen(data);

    snprintf(cmd_url, sizeof(cmd_url), "AT+HTTPPARA=\"URL\",\"%s\"\r\n", HTTP_URL);
    snprintf(cmd_data, sizeof(cmd_data), "AT+HTTPDATA=%d,10000\r\n", body_len);

    for (int i = 0; i < 3; i++) {
        
        if (sim_send_at("AT+HTTPINIT\r\n", "OK", 5000) &&
            sim_send_at(cmd_url, "OK", 5000) &&
            sim_send_at("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n", "OK", 3000) &&
            sim_send_at(cmd_data, "DOWNLOAD", 5000)) 
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            uart_write_bytes(SIM_UART, data, body_len);

            if (sim_send_at("", "OK", 10000) && 
                sim_send_at("AT+HTTPACTION=1\r\n", "OK", 5000)) 
            {
                int http_status = 0;
                if (xQueueReceive(http_queue, &http_status, pdMS_TO_TICKS(10000)) == pdPASS) {
                    ESP_LOGI(TAG, "HTTP Status Code: %d", http_status);
                    
                    if (http_status == 200) {
                        sim_send_at("AT+HTTPTERM\r\n", "OK", 3000);
                        ESP_LOGI(TAG, "HTTP POST COMPLETED!");
                        return true; 
                    }
                }
            }
        }

        sim_send_at("AT+HTTPTERM\r\n", "OK", 2000);
        ESP_LOGW(TAG, "HTTP Retry lần %d...", i + 1);
    }
    
    ESP_LOGE(TAG, "HTTP POST FAILED!");
    return false;
}

void init_uart(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(SIM_UART, &uart_config);
    uart_set_pin(SIM_UART, SIM_TX_PIN, SIM_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(SIM_UART, BUFFER_SIZE * 2, BUFFER_SIZE * 2, 20, &uart_event_queue, 0);

    tx_queue      = xQueueCreate(10, sizeof(char *));
    mqtt_queue    = xQueueCreate(10, sizeof(char *));
    http_queue    = xQueueCreate(5,  sizeof(int));
    at_resp_queue = xQueueCreate(10, sizeof(char *));

    
    xTaskCreate(uart_tx_task, "uart_tx", 4096, NULL, 10, NULL);
    xTaskCreate(uart_rx_router_task, "uart_router", 4096, NULL, 12, NULL);
}

void uart_task(void *pvParameters) {
    init_uart();
    vTaskDelay(pdMS_TO_TICKS(1000));

    for (int i = 0; i < 5; i++) {
        if (sim_send_at("AT\r\n", "OK", 2000)) break;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    sim_send_at("ATZ\r\n", "OK", 3000);
    sim_send_at("AT+CPIN?\r\n", "READY", 3000);
    sim_send_at("AT+CGATT=1\r\n", "OK", 5000);
    sim_send_at("AT+CGDCONT=1,\"IP\",\"v-internet\"\r\n", "OK", 5000);

    sim_send_at("AT+CMQTTDISC=0,60\r\n", "OK", 3000);
    sim_send_at("AT+CMQTTREL=0\r\n", "OK", 3000);
    sim_send_at("AT+CMQTTSTOP\r\n", "OK", 3000);
    vTaskDelay(pdMS_TO_TICKS(2000));

    bool connected = false;
    char cmd[256];
    for (int i = 0; i < 3; i++) {
        if (!sim_send_at("AT+CMQTTSTART\r\n", "+CMQTTSTART: 0", 5000)) continue;

        snprintf(cmd, sizeof(cmd), "AT+CMQTTACCQ=0,\"%s\"\r\n", MQTT_CLIENT_ID);
        if (!sim_send_at(cmd, "OK", 5000)) continue;

        snprintf(cmd, sizeof(cmd), "AT+CMQTTCONNECT=0,\"%s\",%d,1\r\n", MQTT_BROKER_URI, MQTT_KEEPALIVE);
        if (!sim_send_at(cmd, "+CMQTTCONNECT: 0,0", 5000)) continue;

        connected = true;
        break;
    }

    if (!connected) {
        ESP_LOGE(TAG, "MQTT connecting fail, restarting...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    ESP_LOGI(TAG, "MQTT CONNECTED!");

    if (!mqtt_sub_start(MQTT_SUB_TOPIC, 10000)) {
        esp_restart();
    }

    char *mqtt_data;
    while (1) {
        if (xQueueReceive(mqtt_queue, &mqtt_data, portMAX_DELAY) == pdPASS) {
            ESP_LOGI(TAG, "MQTT message: %s", mqtt_data);
            http_confirm_start_charging(mqtt_data);
            free(mqtt_data);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

