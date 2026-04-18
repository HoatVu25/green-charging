#pragma once
#include <stdbool.h>
#include "esp_event_base.h"
#include "stdint.h"
#include "esp_err.h"
#include "esp_http_client.h"


#define MQTT_CLIENT_ID   "GREEN_CHARGING"
#define MQTT_BROKER_URI  "mqtt://103.75.187.153:1885"
#define MQTT_SUB_TOPIC   "GC1765606666"
#define HTTP_URL  "https://api.kullhi.id.vn/charging-post/confirm-start-charging"


typedef void (*Action)(void);
typedef enum{
    NETWORK_CONNECTED,
    NETWORK_DISCONNECTED,
    MQTT_CONNECTED,
    MQTT_DISCONNECTED,
    MQTT_CONNECT_FAIL,
    MQTT_GOT_DATA,
    MQTT_SUBCRIBED,
    HTTP_CONNECTED,
    HTTP_GOT_RESPONSE,
    HTTP_DISCONNECTED
}sim_event;

typedef enum{
    NETWORK_STATE,
    MQTT_STATE,
    HTTP_STATE,
    SIM_READY
}state;

typedef struct{
    state current_state;
    sim_event event;
    Action action;
    state next_state;
}sim_fsm;

typedef enum
{
    OTA_UPDATE_ACTION,
    RESET_CONFIG_ACTION,
    CHARGING_START_ACTION,
    CHARGING_STOP_ACTION,
    PING_ACTION,
    NO_ACTION,
} MqttAction;


void init_sim_fsm(void);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void start_mqtt(void);
static void network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
esp_err_t http_event_handler(esp_http_client_event_t *evt);
void network_connected(void);
void network_disconnected(void);
void action_mqtt_connect_complete(void);
void action_retry_mqtt(void);
void action_mqtt_connect_failed(void);
void http_connected(void);
void http_disconnected(void);
void http_post_data(void);
void http_response(void);
MqttAction handler_action_from_mqtt(const char *data_json);
void handler_sim_fsm(state *curr_state,sim_event evt);
void reciver_event_sim_fsm(void);