idf_component_register(
    SRCS "monitor_control.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_uart
        esp_driver_gpio
)