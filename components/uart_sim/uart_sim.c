#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "uart_sim.h"

#define TAG "UART_SIM"
#define BUF_SIZE 1024


QueueHandle_t event_queue ;

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
    ESP_LOGE(TAG, "FAIL: %s", cmd);
    return false;
}
bool sim_ready(void) {
    if (!sim_send_at("AT\r\n", "OK", 2000)) {
        ESP_LOGI(TAG, "CONNECTING FAIL");
        return false;
    }

    if (sim_send_at("AT+CPIN?\r\n", "READY", 3000)) {
        ESP_LOGI(TAG, "CONNECTING COMPLETED");
        return true;
    }

    return false;
}
bool net_connect() {
    sim_send_at("AT+CSQ\r\n", "OK", 2000);
    
    if (!sim_send_at("AT+CREG?\r\n", "0,1", 2000) && !sim_send_at("AT+CREG?\r\n", "0,5", 2000)) {
        ESP_LOGE(TAG,"NETWORKING FAIL");
        return false;
    }
    
    if (!sim_send_at("AT+CGATT?\r\n", "+CGATT: 1", 3000)) {
        ESP_LOGE(TAG,"NETWORKING FAIL");
        return false; 
    }
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

bool mqtt_connect(void) {
    char cmd_buffer[256];

    sim_send_at("AT+CMQTTDISC=0,60\r\n", "OK", 2000);
    sim_send_at("AT+CMQTTREL=0\r\n", "OK", 2000);
    sim_send_at("AT+CMQTTSTOP\r\n", "OK", 2000);

    if (!sim_send_at("AT+CMQTTSTART\r\n", "+CMQTTSTART: 0", 5000)) {
        return false;
    }

    snprintf(cmd_buffer, sizeof(cmd_buffer), "AT+CMQTTACCQ=0,\"%s\"\r\n", MQTT_CLIENT_ID);
    if (!sim_send_at(cmd_buffer, "OK", 3000)) return false;

    snprintf(cmd_buffer, sizeof(cmd_buffer), "AT+CMQTTCONNECT=0,\"%s\",60,1\r\n", MQTT_BROKER_URI);
    if (!sim_send_at(cmd_buffer, "+CMQTTCONNECT: 0,0", 10000)) {
        ESP_LOGE(TAG, "CONNECTING FAIL");
        return false;
    }

    snprintf(cmd_buffer, sizeof(cmd_buffer), "AT+CMQTTSUB=0,%d,1\r\n", strlen(MQTT_SUB_TOPIC));
    if (sim_send_at(cmd_buffer, ">", 5000)) {
        uart_write_bytes(SIM_UART, MQTT_SUB_TOPIC, strlen(MQTT_SUB_TOPIC)); 
        if (sim_send_at("", "+CMQTTSUB: 0,0", 5000)) {
            ESP_LOGI(TAG, "MQTT CONNECTED ");
            return true;
        }
    }
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

bool mqtt_listen(char *data, int timeout) {
    static char rx_buffer[BUF_SIZE];
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
    memset(rx_buffer, 0, BUFFER_SIZE);

    ESP_LOGI(TAG, "Received data: %s", data);
    return true;
}

bool post_http(const char *url, const char *json_data) {
    char cmd[256];
    int data_len = strlen(json_data);

    sim_send_at("AT+HTTPTERM\r\n", "OK", 1000);
    if (!sim_send_at("AT+HTTPINIT\r\n", "OK", 3000)) {
        return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"\r\n", url);
    if (!sim_send_at(cmd, "OK", 3000)) {
        sim_send_at("AT+HTTPTERM\r\n", "OK", 1000);
        return false;
    }

    if (!sim_send_at("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n", "OK", 3000)) {
        sim_send_at("AT+HTTPTERM\r\n", "OK", 1000);
        return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%d,10000\r\n", data_len);
    if (sim_send_at(cmd, "DOWNLOAD", 5000)) {
        uart_write_bytes(SIM_UART, json_data, data_len);
        if (!sim_send_at("", "OK", 5000)) {
            sim_send_at("AT+HTTPTERM\r\n", "OK", 1000);
            return false;
        }
    } else {
        sim_send_at("AT+HTTPTERM\r\n", "OK", 1000);
        return false;
    }

    if (sim_send_at("AT+HTTPACTION=1\r\n", "+HTTPACTION: 1,200", 15000)) {
        ESP_LOGI(TAG, "POST HTTP COMPLETED!");
        sim_send_at("AT+HTTPTERM\r\n", "OK", 2000);
        return true;
    }
    sim_send_at("AT+HTTPTERM\r\n", "OK", 2000);
    return false;
}

void sim_task(void *pvParameters){
    int retry_count = 0;
    bool success = false;

    while(retry_count <= 3){
        if(sim_ready()){
            success = true;
            break; 
        }
        retry_count++;
        ESP_LOGW(TAG, "RETRY %d/...", retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }

    if(success){
        system_event_t evt = EVT_SIM_READY;
        xQueueSend(event_queue, &evt, portMAX_DELAY);
    } else {
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    while(1){
        vTaskDelay(portMAX_DELAY); 
    }
}

void network_task(void *pvParameters){
    while(1){
        system_event_t evt;
        if(xQueueReceive(event_queue, &evt, portMAX_DELAY)){
            
            if(evt == EVT_SIM_READY){
                int retry_count = 0;
                bool success = false;
                while(retry_count <= 3){
                    if(net_connect()){
                        success = true;
                        break;
                    }
                    retry_count++;
                    ESP_LOGW(TAG, "RETRY %d/...", retry_count);
                    vTaskDelay(pdMS_TO_TICKS(3000));
                }

                if(success){
                    evt = EVT_GPRS_CONNECTED;
                    xQueueSend(event_queue, &evt, portMAX_DELAY);
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
            }
        }
    }
}

void mqtt_task(void *pvParameters){
    while(1){
        system_event_t evt;
        if(xQueueReceive(event_queue, &evt, portMAX_DELAY)){
            
            if(evt == EVT_GPRS_CONNECTED ){
                int retry_count = 0;
                bool success = false;
                while(retry_count <= 3){
                    if (mqtt_connect() && mqtt_sub_start(MQTT_SUB_TOPIC, 5000)){
                        success = true;
                        break;
                    }
                    retry_count++;
                    ESP_LOGI(TAG, "RETRY %d/...", retry_count);
                    vTaskDelay(pdMS_TO_TICKS(3000)); 
                }

                if(success){
                    evt = EVT_MQTT_CONNECTED;
                    xQueueSend(event_queue, &evt, portMAX_DELAY);
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
            }
        }
    }
}

void http_task(void *pvParameters){
    while (1){
        system_event_t evt;
        if(xQueueReceive(event_queue, &evt, portMAX_DELAY)){
            
            if(evt == EVT_MQTT_CONNECTED){
                int retry_count = 0;
                bool success = false;
                while(retry_count <= 3){
                    if (http_connect(HTTP_URL)){
                        success = true;
                        break;
                    }
                    retry_count++;
                    ESP_LOGW(TAG, "RETRY %d/...", retry_count);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                }

                if(success){
                    evt = EVT_GET_MESSAGE_MQTT; 
                    xQueueSend(event_queue, &evt, portMAX_DELAY);
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
            }
        }
    }
}

void uart_init(void){
    event_queue = xQueueCreate(10, sizeof(system_event_t));
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(SIM_UART, &uart_config);
    uart_set_pin(SIM_UART, SIM_TX_PIN, SIM_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(SIM_UART, BUFFER_SIZE * 2, BUFFER_SIZE * 2, 20, NULL, 0);
}

void uart_task(void *pvParameters) {
    uart_init();
    static char mqtt_payload[512];
    int listen_timeout = 3000;
    system_event_t evt;
    if (sim_ready()) {
        evt = EVT_SIM_READY;
        xQueueSend(event_queue, &evt, 0);
    }
    while (1) {
        if (xQueueReceive(event_queue, &evt, portMAX_DELAY)) {
            switch (evt) {
                case EVT_SIM_READY:
                    if (net_connect()) {
                        evt = EVT_GPRS_CONNECTED;
                        xQueueSend(event_queue, &evt, 0);
                    } else {
                        vTaskDelay(pdMS_TO_TICKS(5000));
                        evt = EVT_SIM_READY; 
                        xQueueSend(event_queue, &evt, 0);
                    }
                    break;

                case EVT_GPRS_CONNECTED:
                    if (mqtt_connect()) {
                        if (mqtt_sub_start(MQTT_SUB_TOPIC, 5000)) {
                            evt = EVT_GET_MESSAGE_MQTT; 
                            xQueueSend(event_queue, &evt, 0);
                        }
                    } else {
                        evt = EVT_SIM_READY;
                        xQueueSend(event_queue, &evt, 0);
                    }
                    break;
                case EVT_GET_MESSAGE_MQTT:
                    if (mqtt_listen(mqtt_payload, listen_timeout)) {
                        evt = EVT_POST_HTTP;
                        xQueueSend(event_queue, &evt, 0);
                    } else {
                        evt = EVT_GET_MESSAGE_MQTT;
                        xQueueSend(event_queue, &evt, 0);
                    }
                    break;

                case EVT_POST_HTTP:
                    if (post_http(HTTP_URL, mqtt_payload)) {
                        memset(mqtt_payload, 0, sizeof(mqtt_payload));
                        evt = EVT_GET_MESSAGE_MQTT; 
                        xQueueSend(event_queue, &evt, 0);
                    } else {
                        evt = EVT_GPRS_CONNECTED; 
                        xQueueSend(event_queue, &evt, 0);
                    }
                    break;
                default:
                    break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}