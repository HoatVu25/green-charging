#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "uart_sim.h"


#define TAG "UART_SIM"
QueueHandle_t event_queue;

static bool sim_send_at(const char *cmd, const char *expected, int timeout_ms) {
    uart_flush_input(SIM_UART);
    if (cmd != NULL && strlen(cmd) > 0) {
        uart_write_bytes(SIM_UART, cmd, strlen(cmd));
        ESP_LOGI(TAG, ">> %s", cmd);
    }

    uint8_t rx_buf[512];
    int rx_len = 0; 
    memset(rx_buf, 0, sizeof(rx_buf));

    TickType_t start_tick = xTaskGetTickCount();
    
    while ((xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS < timeout_ms) {
        int space_left = sizeof(rx_buf) - 1 - rx_len;
        if (space_left <= 0) break;
        int len = uart_read_bytes(SIM_UART, rx_buf + rx_len, space_left, pdMS_TO_TICKS(20));
        if (len > 0) {
            rx_len += len;
            rx_buf[rx_len] = '\0'; 
            
            if (strstr((char *)rx_buf, expected) != NULL) {
                ESP_LOGI(TAG, "<< %s", rx_buf);
                return true;
            }
            if (strstr((char *)rx_buf, "ERROR") != NULL) {
                ESP_LOGE(TAG, "<< ERROR: %s", rx_buf);
                return false;
            }
        }
    }
    ESP_LOGE(TAG, "FAIL : %s", cmd);
    return false;
}

bool sim_ready(void) {
    if (!sim_send_at("AT\r\n", "OK", 2000)) {
        ESP_LOGI(TAG, "AT CHECK FAIL");
        return false;
    }
    if (sim_send_at("AT+CPIN?\r\n", "READY", 3000)) {
        ESP_LOGI(TAG, "SIM READY COMPLETED");
        return true;
    }
    return false;
}

bool net_connect() {
    sim_send_at("AT+CSQ\r\n", "OK", 2000);
    if (!sim_send_at("AT+CREG?\r\n", "0,1", 2000) && !sim_send_at("AT+CREG?\r\n", "0,5", 2000)) {
        return false;
    }
    if (!sim_send_at("AT+CGATT?\r\n", "+CGATT: 1", 3000)) {
        return false; 
    }
    return true;
}

bool mqtt_connect(void) {
    char cmd_buffer[256];
    sim_send_at("AT+CMQTTDISC=0,60\r\n", "OK", 2000);
    sim_send_at("AT+CMQTTREL=0\r\n", "OK", 2000);
    sim_send_at("AT+CMQTTSTOP\r\n", "OK", 2000);

    if (!sim_send_at("AT+CMQTTSTART\r\n", "+CMQTTSTART: 0", 5000)) return false;

    snprintf(cmd_buffer, sizeof(cmd_buffer), "AT+CMQTTACCQ=0,\"%s\"\r\n", MQTT_CLIENT_ID);
    if (!sim_send_at(cmd_buffer, "OK", 3000)) return false;

    snprintf(cmd_buffer, sizeof(cmd_buffer), "AT+CMQTTCONNECT=0,\"%s\",60,1\r\n", MQTT_BROKER_URI);
    if (!sim_send_at(cmd_buffer, "+CMQTTCONNECT: 0,0", 10000)) return false;
    return true;
}

bool mqtt_sub_start(const char *topic, int timeout) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CMQTTSUB=0,%d,1\r\n", (int)strlen(topic));
    if (!sim_send_at(cmd, ">", timeout)) return false;
    
    vTaskDelay(pdMS_TO_TICKS(200));
    uart_write_bytes(SIM_UART, topic, strlen(topic));

    if (sim_send_at("", "+CMQTTSUB: 0,0", 5000)) {
        ESP_LOGI(TAG, "MQTT SUBSCRIBED: %s", topic);
        return true;
    }
    return false;
}

bool mqtt_listen(char *data, int timeout) {
    static char rx_buffer[BUFFER_SIZE];
    memset(rx_buffer, 0, BUFFER_SIZE);
    
    int len = uart_read_bytes(SIM_UART, rx_buffer, BUFFER_SIZE - 1, pdMS_TO_TICKS(timeout));
    if (len <= 0) return false;
        
    rx_buffer[len] = '\0';
    char *p = strstr(rx_buffer, "+CMQTTRXPAYLOAD: 0,");
    char *end = strstr(rx_buffer, "+CMQTTRXEND:");

    if (!p || !end) return false;

    int data_size = atoi(p + strlen("+CMQTTRXPAYLOAD: 0,"));
    if (data_size <= 0) return false;

    char *data_start = strstr(p, "\r\n");
    if (!data_start) return false;
    data_start += 2;

    memcpy(data, data_start, data_size);
    data[data_size] = '\0';

    ESP_LOGI(TAG, "Received payload: %s", data);
    return true;
}

bool http_connect(const char *url) {
    char cmd_buffer[256];
    sim_send_at("AT+HTTPTERM\r\n", "OK", 1000);

    if (!sim_send_at("AT+HTTPINIT\r\n", "OK", 3000)) {
        return false;
    }

    snprintf(cmd_buffer, sizeof(cmd_buffer), "AT+HTTPPARA=\"URL\",\"%s\"\r\n", url);
    if (!sim_send_at(cmd_buffer, "OK", 3000)) {
        sim_send_at("AT+HTTPTERM\r\n", "OK", 1000);
        return false;
    }

    if (!sim_send_at("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n", "OK", 3000)) {
        sim_send_at("AT+HTTPTERM\r\n", "OK", 1000);
        return false;
    }

    sim_send_at("AT+HTTPTERM\r\n", "OK", 2000);
    return true;
}

bool post_http(const char *url, const char *json_data) {
    bool success = false;
    char cmd[256];
    int data_len = strlen(json_data);

    sim_send_at("AT+HTTPTERM\r\n", "OK", 1000); 

    if (sim_send_at("AT+HTTPINIT\r\n", "OK", 3000)) {
        
        snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"\r\n", url);
        if (sim_send_at(cmd, "OK", 3000) && 
            sim_send_at("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n", "OK", 3000)) {
            
            snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%d,10000\r\n", data_len);
            if (sim_send_at(cmd, "DOWNLOAD", 5000)) {
                uart_write_bytes(SIM_UART, json_data, data_len);
                
                if (sim_send_at("", "OK", 5000)) {
                    if (sim_send_at("AT+HTTPACTION=1\r\n", "+HTTPACTION: 1,200", 15000)) {
                        ESP_LOGI(TAG, "POST HTTP COMPLETED SUCCESSFULLY!");
                        success = true;
                    }
                }
            }
        }
    }
    sim_send_at("AT+HTTPTERM\r\n", "OK", 2000); 
    return success;
}


bool network_check(void) {
    network_event_t state;
    network_event_t retry_target = NET_EVT_INIT;
    xQueueReceive(event_queue,&state,portMAX_DELAY);
    int retry_count = 0;

    while (state != NET_EVT_READY) {
        switch (state) {
            case NET_EVT_INIT:
                if (sim_ready()) {
                    state = NET_EVT_WAIT_GPRS;
                    retry_count = 0; 
                } else {
                    retry_target = NET_EVT_INIT; 
                    state = NET_EVT_RETRY;           
                }
                break;

            case NET_EVT_WAIT_GPRS:
                if (net_connect()) {
                    state = NET_EVT_READY;
                } else {
                    retry_target = NET_EVT_WAIT_GPRS;
                    state = NET_EVT_RETRY;
                }
                break;
            case NET_EVT_RETRY:
                retry_count++;
                if (retry_count > 3) {
                    state = NET_EVT_ERROR;
                } else {
                    ESP_LOGW(TAG, "NETWORK RETRY %d...", retry_count);
                    vTaskDelay(pdMS_TO_TICKS(3000)); 
                    state = retry_target;            
                }
                break;
            case NET_EVT_ERROR:
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart(); 
            default: break;
        }
    }
    return (state == NET_EVT_READY);
}

bool mqtt_check(void) {
    mqtt_event_t state; 
    mqtt_event_t retry_target = MQTT_EVT_CONNECT;
    xQueueReceive(event_queue,&state,portMAX_DELAY);
    int retry_count = 0;

    while (state != MQTT_EVT_READY) {
        switch (state) {

            case MQTT_EVT_CONNECT:
                if (mqtt_connect()) {
                    state = MQTT_EVT_SUBSCRIBE;
                    retry_count = 0;
                } else {
                    retry_target = MQTT_EVT_CONNECT;
                    state = MQTT_EVT_RETRY;
                }
                break;

            case MQTT_EVT_SUBSCRIBE:
                if (mqtt_sub_start(MQTT_SUB_TOPIC, 5000)) {
                    state = MQTT_EVT_READY; 
                } else {
                    retry_target = MQTT_EVT_SUBSCRIBE;
                    state = MQTT_EVT_RETRY;
                }
                break;
            case MQTT_EVT_RETRY:
                retry_count++;
                if (retry_count > 3) {
                    state = MQTT_EVT_ERROR;
                } else {
                    ESP_LOGW(TAG, "MQTT RETRY %d ...", retry_count);
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    state = retry_target;
                }
                break;
            case MQTT_EVT_ERROR:
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            default: break;
        }
    }
    return (state == MQTT_EVT_READY);
}

bool http_check(void) {
    http_event_t state;
    http_event_t retry_target = HTTP_EVT_CONNECT;
    int retry_count = 0;
    xQueueReceive(event_queue,&state,portMAX_DELAY);

    while (state != HTTP_EVT_READY) {
        switch (state) {
            case HTTP_EVT_CONNECT:
                if (http_connect(HTTP_URL)) {
                    state = HTTP_EVT_READY;
                    retry_count = 0;
                } else {
                    retry_target = HTTP_EVT_CONNECT;
                    state = HTTP_EVT_RETRY;
                }
                break;
            case HTTP_EVT_RETRY:
                retry_count++;
                if (retry_count > 3) {
                    state = HTTP_EVT_ERROR;
                } else {
                    ESP_LOGW(TAG, "HTTP RETRY %d...", retry_count);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    state = retry_target;
                }
                break;
            case HTTP_EVT_ERROR:
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            default: 
                break;
        }
    }
    sim_send_at("AT+HTTPTERM\r\n", "OK", 1000);
    return (state == HTTP_EVT_READY); 
}

void uart_init(void){
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(SIM_UART, &uart_config);
    uart_set_pin(SIM_UART, SIM_TX_PIN, SIM_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(SIM_UART, BUFFER_SIZE * 2, BUFFER_SIZE * 2, 0, NULL, 0); 
}

void uart_task(void *pvParameters) {
    uart_init();
    static char mqtt_payload[BUFFER_SIZE];
    memset(mqtt_payload, 0, sizeof(mqtt_payload));

    event_sys evt_sys;
    evt_sys.net_evt = NET_EVT_INIT; 
    xQueueSend(event_queue,&evt_sys.net_evt,portMAX_DELAY);

    if (!network_check()) {
        ESP_LOGE(TAG, "NETWORK FAIL!");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    else {
        evt_sys.mqtt_evt = MQTT_EVT_CONNECT;
        xQueueSend(event_queue,&evt_sys.net_evt,portMAX_DELAY);
    }
    if (!mqtt_check()) {
        ESP_LOGE(TAG, "MQTT FAIL!");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    else{
        evt_sys.mqtt_evt = HTTP_EVT_CONNECT;
        xQueueSend(event_queue,&evt_sys.net_evt,portMAX_DELAY);
    }
    if (!http_check()) {
        ESP_LOGE(TAG, "HTTP FAIL!");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    while (1) {
        if (evt_sys.net_evt == NET_EVT_READY && evt_sys.mqtt_evt == MQTT_EVT_READY && evt_sys.http_evt == HTTP_EVT_READY){
            if (mqtt_listen(mqtt_payload, 3000)) {
                if (post_http(HTTP_URL, mqtt_payload)) {
                    memset(mqtt_payload, 0, sizeof(mqtt_payload));
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart(); 
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}