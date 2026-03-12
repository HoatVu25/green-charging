idf_component_register(
    SRCS "wifi_provisoning.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash 
        esp_wifi
        esp_event 
        esp_netif 
        wifi_provisioning
        bt 
        log
)