#include <stdio.h>
#include "wifi_provision.h"
#include "uart_sim.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "monitor_control.h"

#define BUFFER_SIZE 1024
void app_main(void)
{
    xTaskCreate(reciver_event_sim_fsm, "uart_sim_task",BUFFER_SIZE * 10, NULL, 5, NULL);
    //xTaskCreate(reciver_event_state_machine,"FSM WIFI_PROVISIONING", BUFFER_SIZE * 8, NULL , 6 , NULL);
    //xTaskCreate(data_monitor_task, "monitor_task",BUFFER_SIZE * 2, NULL, 5, NULL);
}
