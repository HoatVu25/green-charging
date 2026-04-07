#include <stdio.h>
// #include "wifi_provisioning.h"
#include "uart_sim.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    xTaskCreate(uart_task, "uart_sim_task",BUFFER_SIZE * 10, NULL, 5, NULL);
    // wifi_provisioning_init();
}
