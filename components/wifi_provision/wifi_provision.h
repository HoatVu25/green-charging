#pragma once

#include <stdbool.h>

#define PROV_SERVICE_NAME_PREFIX    "PROV_"
#define APP_PROOF_OF_POSSESSION     "abcd1234"


typedef void (*Action)(void);

typedef enum{
    WIFI_PROV_BEGIN,
    WIFI_PROV_RECV,
    WIFI_PROV_FAIL,
    WIFI_PROV_SUCCESS,
    WIFI_PROV_FINISH
} event_wifi_provisioning;

typedef struct{
    event_wifi_provisioning event;
    Action action;
} wifi_prov_FSM;


void wifi_provisioning_init(void);
bool wifi_is_provisioned(void);
void reciver_event_state_machine(void);
void handler_state_machine(event_wifi_provisioning evt);