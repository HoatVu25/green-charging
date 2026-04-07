#pragma once
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define SIM_UART UART_NUM_0
#define SIM_TX_PIN 43
#define SIM_RX_PIN 44


#define MQTT_CLIENT_ID   "GREEN_CHARGING"
#define MQTT_BROKER_URI  "tcp://103.75.187.153:1885"
#define MQTT_KEEPALIVE   60
#define MQTT_SUB_TOPIC   "GC1765606666"
#define HTTP_URL  "https://api.kullhi.id.vn/charging-post/confirm-start-charging"


void uart_task();
bool mqtt_publish(const char *topic, const char *data);
bool mqtt_sub_start(const char *topic, int timeout_ms);
bool mqtt_listen(char *data, int timeout);
bool http_confirm_start_charging(const char *data);


