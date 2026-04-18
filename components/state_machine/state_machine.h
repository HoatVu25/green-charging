

// #ifndef _STATE_MACHINE_H_
// #define _STATE_MACHINE_H_

// #include <stdio.h>
// #include <freertos/FreeRTOS.h>
// #include "freertos/queue.h"

// #define FSM_TABLE_SIZE 20

// typedef void (*FsmAction)(void);

// typedef enum {
// 	STATE_BOOT,
// 	STATE_CONFIG,
// 	STATE_OTA,
// 	STATE_INTERNET,
// 	STATE_MQTT,
// 	STATE_IDLE,
// 	STATE_CONFIRM,
// 	STATE_CHARGE,
// 	STATE_PUBLISH,
// 	STATE_LOG_BUG,
// 	STATE_RESTART,
// 	STATE_MAX
// } FsmState;

// typedef enum {
// 	EVENT_DATA_VALID,
// 	EVENT_DATA_INVALID,
// 	EVENT_CONFIG_COMPLETE,
// 	EVENT_FLAG_OTA_VALID,
// 	EVENT_FLAG_OTA_UNVALID,
// 	EVENT_START_NET,
// 	EVENT_RETRY_NET,
// 	EVENT_INTERNET_COMPLETE,
// 	EVENT_INTERNET_FAILED,
// 	EVENT_START_MQTT,
// 	EVENT_RETRY_MQTT,
// 	EVENT_MQTT_CONNECT_COMPLETE,
// 	EVENT_MQTT_CONNECT_FAILED,
// 	EVENT_CHARGING_START,
// 	EVENT_CONFIRM_CHARGING_COMPLETE,
// 	EVENT_CONTINUE_CHARGE,
// 	EVENT_CHARGING_STOP,
// 	EVENT_PUBLISH_COMPLETE,
// 	EVENT_LOG_BUG_END,
// 	EVENT_RESET_CONFIG
// } FsmEvent;

// typedef struct
// {
//     State from;
//     Action action;
//     EventSignal event;
//     State to;
// } ChargeStationFSM;

// extern QueueHandle_t fsm_queue;
// extern void (*FsmEntryActions[STATE_MAX])(void);
// extern FsmTableEntry FSM_Table[FSM_TABLE_SIZE];

// void fsm_init_entry_actions(void);
// void fsm_init_table(void);
// void fsm_event_receiver(void *param);
// void fsm_handle_event(FsmState *current_state, FsmEvent event);

// #endif
