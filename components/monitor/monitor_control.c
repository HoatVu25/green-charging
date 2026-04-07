// #include "stdio.h"
// #include "driver/uart.h"
// #include "driver/gpio.h"
// #include "monitor_control.h"
// #include "esp_log.h"
// #include "esp_err.h"

// static const char *TAG = "UART_MONITOR";

// void uart_init(void){
//     uart_config_t uart_config ={
//         .baud_rate = 9600,
//         .data_bits = UART_DATA_8_BITS,
//         .parity = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
//         .rx_flow_ctrl_thresh = 122,
//     };
//     ESP_ERROR_LOG(uart_param_config(MONITOR_UART, &uart_config));
//     ESP_ERROR_LOG(uart_set_pin(MONITOR_UART, MONITOR_TX_PIN, MONITOR_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
//     ESP_ERROR_LOG(uart_driver_install(MONITOR_UART, BUFFER_SIZE * 4, BUFFER_SIZE * 4, 0, NULL, 0));
// }


// void data_monitor(){
//     uart_init();
//     char *data=&charging_port_1; 
// }