#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"         
#include "esp_netif_defaults.h"    
#include "esp_modem_api.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_http_client.h"
#include "esp_modem_dce_config.h"
#include "esp_modem_config.h"
#include "uart_sim.h"
#include "driver/gpio.h"
#include "esp_crt_bundle.h"
#include "driver/uart.h"
#include <cJSON.h>         

#define BUFFER_SIZE 1024
#define SIM_UART UART_NUM_0
#define SIM_TX_PIN 43
#define SIM_RX_PIN 44
static const char *TAG = "SIM7670_PROJ";
#define LENGTH_PROGRESS 9
QueueHandle_t sim_queue = NULL;
sim_event sim_evt;

sim_fsm Table_FSM[LENGTH_PROGRESS];

void init_sim_fsm(void){
    Table_FSM[0]=(sim_fsm){NETWORK_STATE , NETWORK_CONNECTED , network_connected , MQTT_STATE};
    Table_FSM[1]=(sim_fsm){NETWORK_STATE , NETWORK_DISCONNECTED , network_disconnected , NETWORK_STATE};
    Table_FSM[2]=(sim_fsm){MQTT_STATE , MQTT_CONNECTED , action_mqtt_connect_complete , SIM_READY};
    Table_FSM[3]=(sim_fsm){MQTT_STATE , MQTT_DISCONNECTED , action_retry_mqtt , MQTT_STATE};
    Table_FSM[4]=(sim_fsm){MQTT_STATE , MQTT_CONNECT_FAIL , action_mqtt_connect_failed , MQTT_STATE};
    Table_FSM[7]=(sim_fsm){SIM_READY , MQTT_GOT_DATA , http_post_data , SIM_READY};
    Table_FSM[8]=(sim_fsm){SIM_READY , HTTP_GOT_RESPONSE , http_response , SIM_READY};
}

void network_connected(void){
    ESP_LOGI(TAG,"NETWORK CONNECTED");
    start_mqtt();
}

void network_disconnected(void){
    ESP_LOGI(TAG,"NETWORK DISCONNECTED");
    esp_restart();
}

void action_mqtt_connect_complete(void)
{
    ESP_LOGI(TAG,"MQTT CONNECTED");
}

void action_retry_mqtt(void){
   ESP_LOGI("FSM_ACTION", "RETRY_MQTT → Restarting MQTT client...");
   start_mqtt();
}

void action_mqtt_connect_failed(void)
{
    ESP_LOGI("FSM_ACTION", "MQTT_CONNECT_FAILED → Logging bug or fallback");
}

void http_connected(void){
    ESP_LOGI(TAG,"HTTP CONNECTED");
}

void http_disconnected(void){
    ESP_LOGI(TAG,"HTTP DISCONNECTED");
}

void http_post_data(void) {
    // DATA TEST POST HTTP
    char post_data[512];
    const char* postCode = "GC59286691881";
    int state = 2;
    const char* paymentDriverCode = "GC8PHV3MEQ";
    int maxAmount = 14877;
    long long sessionCode = 1775403288501;
    int pricePost = 10000;

    snprintf(post_data, sizeof(post_data),"{\"postCode\":\"%s\",\"state\":%d,\"paymentDriverCode\":\"%s\",\"maxAmount\":%d,\"sessionCode\":%lld,\"pricePost\":%d}",postCode, state, paymentDriverCode, maxAmount, sessionCode, pricePost);

    esp_http_client_config_t config = {
        .url = HTTP_URL,                
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,            
        .buffer_size = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach, 
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST COMPLETED, Status Code = %d", status_code);
    } else {
        ESP_LOGE(TAG, "HTTP POST FAIL: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void http_response(void){
    ESP_LOGI(TAG,"HTTP GOT RESPONSE");
}

MqttAction handler_action_from_mqtt(const char *data_json)
{
    cJSON *root = cJSON_Parse(data_json);
    if (root == NULL)
    {
        ESP_LOGE("JSON", "BUG");
    }
    cJSON *action = cJSON_GetObjectItem(root, "state");
    ESP_LOGI("Value Action", "%d", action->valueint);
    MqttAction action_handler = action->valueint;
    cJSON_Delete(root);
    switch (action_handler)
    {
    case 0:
        return OTA_UPDATE_ACTION;
    case 1:
        return RESET_CONFIG_ACTION;
    case 2:
        return CHARGING_START_ACTION;
    case 3:
        return CHARGING_STOP_ACTION;
    default:
        return NO_ACTION;
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    MqttAction mqtt_act;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            esp_mqtt_client_subscribe(client, MQTT_SUB_TOPIC, 1);
            sim_evt = MQTT_CONNECTED;
            xQueueSend(sim_queue,&sim_evt,portMAX_DELAY);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "PAYLOAD: %.*s", event->data_len, event->data);
            // mqtt_act = handler_action_from_mqtt(event->data);
            // xQueueSend(sim_queue,mqtt_act,portMAX_DELAY);
            // http_post_data();
            sim_evt = MQTT_GOT_DATA;
            xQueueSend(sim_queue,&sim_evt,portMAX_DELAY);
            break;
        case MQTT_EVENT_DISCONNECTED:
            sim_evt = MQTT_DISCONNECTED;
            xQueueSend(sim_queue,&sim_evt,portMAX_DELAY);
            break;
        case MQTT_EVENT_ERROR:
            sim_evt = MQTT_CONNECT_FAIL;
            xQueueSend(sim_queue,&sim_evt,portMAX_DELAY);
            break;
        default:
            break;
    }
}

static void start_mqtt(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI, 
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

static void network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_PPP_GOT_IP: {
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                ESP_LOGI(TAG, "GET IP = " IPSTR, IP2STR(&event->ip_info.ip));
                sim_evt = NETWORK_CONNECTED;
                xQueueSend(sim_queue,&sim_evt,portMAX_DELAY);
                break;
            }
            case IP_EVENT_PPP_LOST_IP:
                sim_evt = NETWORK_DISCONNECTED;
                xQueueSend(sim_queue,&sim_evt,portMAX_DELAY);
                break;
            default:
                break;
        }
    }
}


esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        // case HTTP_EVENT_ERROR: 
        //     ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        //     break;
        // case HTTP_EVENT_ON_CONNECTED:
        //     sim_evt = HTTP_CONNECTED;
        //     xQueueSend(sim_queue,&sim_evt,portMAX_DELAY);
        //     break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ESP_LOGI(TAG, "Server phản hồi: %.*s", evt->data_len, (char*)evt->data);
            }
            sim_evt = HTTP_GOT_RESPONSE;
            xQueueSend(sim_queue,&sim_evt,portMAX_DELAY);
            break;
        // case HTTP_EVENT_DISCONNECTED:
        //     sim_evt = HTTP_DISCONNECTED;
        //     xQueueSend(sim_queue,&sim_evt,portMAX_DELAY);
        //     break;
        default:
            break;
    }
    return ESP_OK;
}


void init_sim(void) {
    /// RESET SIM AT COMMAND
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };
    uart_param_config(UART_NUM_0,&uart_config);
    uart_set_pin(UART_NUM_0 , 43 , 44 , UART_PIN_NO_CHANGE , UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_0 , BUFFER_SIZE*2 , BUFFER_SIZE*2 , 0 , NULL , 0);

    vTaskDelay(pdMS_TO_TICKS(1000));                  
    uart_write_bytes(UART_NUM_0, "+++",3);           
    vTaskDelay(pdMS_TO_TICKS(1000));
    uart_flush(UART_NUM_0);
    uart_write_bytes(UART_NUM_0, "AT+CRESET\r\n", strlen("AT+CRESET\r\n"));
    static char res[128];
    int len = uart_read_bytes(UART_NUM_0,res , sizeof(res)-1 , pdMS_TO_TICKS(2000));
    if (len > 0 && strstr(res , "OK")){
        ESP_LOGE(TAG,"RESPONSE RESET: %s",res);
    }
    else{
        ESP_LOGE(TAG,"RESET FAIL");
        esp_restart();
    }
    vTaskDelay(5000/portTICK_PERIOD_MS);
    uart_driver_delete(UART_NUM_0);

    /// CONFIG MODEM SIM
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, network_event_handler, NULL));
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.port_num = UART_NUM_0;
    dte_config.uart_config.tx_io_num = 43;
    dte_config.uart_config.rx_io_num = 44;


    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("v-internet");
    esp_modem_dce_t *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, esp_netif);

    
    esp_err_t err = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    if (err == ESP_OK) {
    } else {
        ESP_LOGE(TAG, "ERROR : %s", esp_err_to_name(err));     
        esp_restart();
    }
}

void handler_sim_fsm(state *curr_state,sim_event evt){
    for(int i=0 ; i < LENGTH_PROGRESS ; i++){
        if (Table_FSM[i].current_state == *curr_state && Table_FSM[i].event == evt){
            Table_FSM[i].action();
            *curr_state = Table_FSM[i].next_state;
            break;
        }
    }
}

void reciver_event_sim_fsm(void){
    sim_queue = xQueueCreate(10,sizeof(sim_event));
    if (sim_queue == NULL)
    {
        ESP_LOGI("QUEUE", "Failed");
        return;
    }
    init_sim();
    init_sim_fsm();
    static state curr_state = NETWORK_STATE;
    sim_event evt;
    while (1)
    {
        if (xQueueReceive(sim_queue,&evt,portMAX_DELAY) == pdPASS){
           handler_sim_fsm(&curr_state,evt);
        }
        else{
                ESP_LOGI("QUEUE", " !Complete");
        }
    }
}