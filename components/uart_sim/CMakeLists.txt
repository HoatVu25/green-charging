idf_component_register(
    SRCS "uart_sim.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_driver_gpio
        esp_driver_uart
        mqtt 
        esp_http_client
        esp_event
        esp_netif
        esp_modem
        json
)