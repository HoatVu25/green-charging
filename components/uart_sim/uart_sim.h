#pragma once
#include <stdbool.h>
#include "esp_event_base.h"
#include "stdint.h"
#include "esp_err.h"
#include "esp_http_client.h"

#define BUFFER_SIZE 1024
#define SIM_UART UART_NUM_0
#define SIM_TX_PIN 43
#define SIM_RX_PIN 44


#define MQTT_CLIENT_ID   "GREEN_CHARGING"
#define MQTT_BROKER_URI  "mqtt://103.75.187.153:1885"
#define MQTT_SUB_TOPIC   "GC1765606666"
#define HTTP_URL  "https://api.kullhi.id.vn/charging-post/confirm-start-charging"


void uart_task(void);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void start_mqtt(void);
void http_post_data(void);
static void network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
esp_err_t http_event_handler(esp_http_client_event_t *evt);


