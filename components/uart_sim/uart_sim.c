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
          
#define SIM_RST_PIN 4
static const char *TAG = "SIM7670_PROJ";

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

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT CONNECTED!");
            int msg_id = esp_mqtt_client_subscribe(client, MQTT_SUB_TOPIC, 1);
            ESP_LOGI(TAG, "Request subscribe topic '%s', msg_id=%d", MQTT_SUB_TOPIC, msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "PAYLOAD: %.*s", event->data_len, event->data);
             http_post_data();
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT DISCONNECTED");
                break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT SUBSCRIBED");
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
                ESP_LOGI(TAG, "START MQTT...");
                start_mqtt();
                break;
            }
            case IP_EVENT_PPP_LOST_IP:
                ESP_LOGE(TAG, "DISCONNECTED");
                break;
            default:
                break;
        }
    }
}


esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ESP_LOGI(TAG, "Server phản hồi: %.*s", evt->data_len, (char*)evt->data);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}


void uart_task(void) {
    /// RESET SIM
    gpio_reset_pin(SIM_RST_PIN);
    gpio_set_direction(SIM_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SIM_RST_PIN, 0); 
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(SIM_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    
    /// CONFIG MODEM SIM
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, network_event_handler, NULL));
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.port_num = UART_NUM_2;
    dte_config.uart_config.tx_io_num = 17;
    dte_config.uart_config.rx_io_num = 16;


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
    
    while(1){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

