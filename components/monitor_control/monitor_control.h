#ifndef MONITOR_CONTROL
#define MONITOR_CONTROL

#include "stdio.h"
#include "stdint.h"

#define MONITOR_TX_PIN 17
#define MONITOR_RX_PIN 18
#define BAUDRATE    9600
#define BUFF_SIZE   1024
#define MONITOR_UART UART_NUM_1

typedef struct {
    int voltage;   
    float current;   
    int power;
    uint32_t time;     
    bool stage_charging;
} charging_data_t;

// charging_data_t charging_port_1;
// charging_data_t charging_port_2;

void data_monitor_task(void *pvParameters);
void uart_init();


#endif