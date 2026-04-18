#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include "wifi_provision.h"

#define WIFI_CONNECTED_BIT  BIT0
#define LENGTH_PROGRESS 5
static EventGroupHandle_t s_wifi_event_group;
static const char *TAG = "wifi_prov";
wifi_prov_FSM FSM_Table[LENGTH_PROGRESS];
QueueHandle_t wifi_prov_queue = NULL;

/// action 
void wifi_prov_start(){
    ESP_LOGI(TAG, "Provisioning");
}
void recv_config_wifi(){
    // wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
    // ESP_LOGI(TAG, "SSID: %s", (const char *)wifi_sta_cfg->ssid);
    ESP_LOGI(TAG, "Got config");
}
void wifi_prov_fail(){
    // wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
    // ESP_LOGE(TAG, "Provisioning failed: %s",(*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Sai mật khẩu WiFi": "Không tìm thấy AP");
    wifi_prov_mgr_reset_sm_state_on_failure();
}
void wifi_prov_success(){
    ESP_LOGI(TAG, "Provisioning completed");
}
void wifi_prov_end(){
    wifi_prov_mgr_deinit();
}

void init_wifi_prov_FSM(void){
    FSM_Table[0]=(wifi_prov_FSM){WIFI_PROV_BEGIN,wifi_prov_start};
    FSM_Table[1]=(wifi_prov_FSM){WIFI_PROV_RECV,recv_config_wifi};
    FSM_Table[2]=(wifi_prov_FSM){WIFI_PROV_FAIL,wifi_prov_fail};
    FSM_Table[3]=(wifi_prov_FSM){WIFI_PROV_SUCCESS,wifi_prov_success};
    FSM_Table[4]=(wifi_prov_FSM){WIFI_PROV_FINISH,wifi_prov_end};
}

void handler_state_machine(event_wifi_provisioning evt){
    for (int i=0;i<LENGTH_PROGRESS;i++){
        if (FSM_Table[i].event == evt){
            FSM_Table[i].action();
        }
    }
}

void reciver_event_state_machine(void){
    wifi_prov_queue = xQueueCreate(10, sizeof(event_wifi_provisioning));
	if (wifi_prov_queue == NULL) {
		ESP_LOGE("FSM", "Queue create failed");
		return;
	}
    wifi_provisioning_init();
    init_wifi_prov_FSM();
    event_wifi_provisioning evt;
	while (1) {
		if (xQueueReceive(wifi_prov_queue, &evt, portMAX_DELAY) == pdPASS) {
			ESP_LOGI("FSM", "Received event: %d", evt);
			handler_state_machine(evt);
		}
	}
}


/// Provisioning events
static void event_handler(void *arg, esp_event_base_t event_base,int32_t event_id, void *event_data){
    event_wifi_provisioning evt;
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                evt = WIFI_PROV_BEGIN;
                xQueueSend(wifi_prov_queue,&evt,portMAX_DELAY);
                break;
            case WIFI_PROV_CRED_RECV: {
                // wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                // ESP_LOGI(TAG, "SSID: %s", (const char *)wifi_sta_cfg->ssid);
                evt = WIFI_PROV_RECV;
                xQueueSend(wifi_prov_queue,&evt,portMAX_DELAY);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                // wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                // ESP_LOGE(TAG, "Provisioning failed: %s",(*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Sai mật khẩu WiFi": "Không tìm thấy AP");
                // wifi_prov_mgr_reset_sm_state_on_failure();
                evt = WIFI_PROV_FAIL;
                xQueueSend(wifi_prov_queue,&evt,portMAX_DELAY);
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                evt = WIFI_PROV_SUCCESS;
                xQueueSend(wifi_prov_queue,&evt,portMAX_DELAY);
                break;

            case WIFI_PROV_END:
                // wifi_prov_mgr_deinit();
                evt = WIFI_PROV_FINISH;
                xQueueSend(wifi_prov_queue,&evt,portMAX_DELAY);
                break;

            default:
                break;
        }

    } else if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "Connecting...");
            esp_wifi_connect();
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) { /// Get ID to format service name and wait until wifi connected
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/// Check provisioning state
bool wifi_is_provisioned(void)
{
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
    return provisioned;
}

/// Connect wifi
void wifi_provisioning_init(void)
{
    /// Check config wifi in NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,   &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,      ESP_EVENT_ANY_ID,   &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,        IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    s_wifi_event_group = xEventGroupCreate();

    wifi_prov_mgr_config_t config = {
        .scheme               = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    if (wifi_is_provisioned()) {
        ESP_LOGI(TAG, "Provisioned");
        wifi_prov_mgr_deinit();

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    } 
    else {
        ESP_LOGI(TAG, "Provisioning...");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        uint8_t eth_mac[6];
        esp_read_mac(eth_mac, ESP_MAC_WIFI_STA);
        char service_name[16];
        snprintf(service_name, sizeof(service_name), "%s%02X%02X%02X",PROV_SERVICE_NAME_PREFIX,eth_mac[3], eth_mac[4], eth_mac[5]);

        ESP_LOGI(TAG, "BLE service name: %s", service_name);

        wifi_prov_security_t security = WIFI_PROV_SECURITY_0;
        // const char *pop = APP_PROOF_OF_POSSESSION;

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security,NULL, service_name, NULL));
    }
}


