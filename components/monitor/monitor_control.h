#ifndef MONITOR_CONTROL
#define MONITOR_CONTROL

#include "stdio.h"
#include "stdint.h"



#define UART_PIN_TX 17
#define UART_PIN_RX 18
#define BAUDRATE    9600
#define BUFF_SIZE   1024


typedef struct {
    int voltage;   
    float current;   
    int power;
    uint32_t time;     
    bool stage_charging;
} charging_port_data_t;


void data_monitor();



#endif