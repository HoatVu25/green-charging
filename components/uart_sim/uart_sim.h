#pragma once
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define SIM_UART UART_NUM_0
#define SIM_TX_PIN 43
#define SIM_RX_PIN 44


#define MQTT_CLIENT_ID   "GREEN_CHARGING"
#define MQTT_BROKER_URI  "tcp://103.75.187.153:1885"
#define MQTT_SUB_TOPIC   "GC1765606666"
#define HTTP_URL  "https://api.kullhi.id.vn/charging-post/confirm-start-charging"


void uart_task();
bool mqtt_publish(const char *topic, const char *data);
bool mqtt_sub_start(const char *topic, int timeout_ms);
bool mqtt_listen(char *data, int timeout);
bool http_confirm_start_charging(const char *data);

typedef enum {
    NET_EVT_IDLE,
    NET_EVT_INIT,
    NET_EVT_WAIT_GPRS,
    NET_EVT_READY,
    NET_EVT_RETRY,       
    NET_EVT_ERROR
} network_event_t;

typedef enum {  
    MQTT_EVT_IDLE,    
    MQTT_EVT_CONNECT,
    MQTT_EVT_SUBSCRIBE,
    MQTT_EVT_READY,
    MQTT_EVT_LISTEN,
    MQTT_EVT_RETRY,      
    MQTT_EVT_ERROR
} mqtt_event_t;

typedef enum {
    HTTP_EVT_IDLE,
    HTTP_EVT_CONNECT,
    HTTP_EVT_READY,
    HTTP_EVT_POST_DATA,
    HTTP_EVT_RETRY,
    HTTP_EVT_ERROR
} http_event_t;


typedef struct {
    network_event_t net_evt;
    mqtt_event_t mqtt_evt;
    http_event_t http_evt;
} event_sys;
