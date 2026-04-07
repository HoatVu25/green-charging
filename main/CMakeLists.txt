idf_component_register(
    SRCS "green_charge_firmware.c"
    INCLUDE_DIRS "."
    REQUIRES  wifi_prov 
              uart_sim
)
