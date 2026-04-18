#include "stdio.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "monitor_control.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"

static const char *TAG = "MONITOR";

void uart_init(void){
    uart_config_t uart_config ={
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
    };
    uart_param_config(MONITOR_UART, &uart_config);
    uart_set_pin(MONITOR_UART, MONITOR_TX_PIN, MONITOR_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(MONITOR_UART, BUFF_SIZE * 4, BUFF_SIZE * 4, 0, NULL, 0);
}

void data_monitor_task(void *pvParameters) {

    char data[128];
    int port = 1, vol = 5, pow = 12;
    float cur = 1.5;
    int state;

    while (1) {
        state = 1;
        snprintf(data, sizeof(data), "PN:%d S:%d V:%d I:%.1f P:%d", port, state, vol, cur, pow);
        
        uart_write_bytes(MONITOR_UART, data, strlen(data));
        printf("Sent: %s\n", data); 

        vTaskDelay(pdMS_TO_TICKS(60000));

        state = 0;
        snprintf(data, sizeof(data), "PN:%d S:%d V:%d I:%.1f P:%d", port, state, vol, cur, pow);
        
        uart_write_bytes(MONITOR_UART, data, strlen(data));
        printf("Sent: %s\n", data);

        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}