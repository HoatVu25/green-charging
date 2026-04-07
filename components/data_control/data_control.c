// #include "stdio.h"
// #include "driver/gpio.h"
// #include "driver/uart.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"

// static const char *TAG = "PZEM_TASK";

// void uart_init(void){
//     uart_config_t uart_config ={
//         .baud_rate = 9600,
//         .data_bits = UART_DATA_8_BITS,
//         .parity = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
//         .rx_flow_ctrl_thresh = 122,
//     };
//     uart_set_pin(UART_NUM);
// }