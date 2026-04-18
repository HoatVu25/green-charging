

// #include "state_machine.h"
// #include "esp_log.h"

// QueueHandle_t fsm_queue = NULL;
// void (*FsmEntryActions[STATE_MAX])(void) = {0};
// FsmTableEntry FSM_Table[FSM_TABLE_SIZE];

// // Dummy entry actions (implement thực tế tuỳ logic)
// void entry_boot(void) { ESP_LOGI("FSM", "Entry BOOT"); }
// void entry_config(void) { ESP_LOGI("FSM", "Entry CONFIG"); }
// void entry_ota(void) { ESP_LOGI("FSM", "Entry OTA"); }
// void entry_internet(void) { ESP_LOGI("FSM", "Entry INTERNET"); }
// void entry_mqtt(void) { ESP_LOGI("FSM", "Entry MQTT"); }
// void entry_idle(void) { ESP_LOGI("FSM", "Entry IDLE"); }
// void entry_confirm(void) { ESP_LOGI("FSM", "Entry CONFIRM"); }
// void entry_charge(void) { ESP_LOGI("FSM", "Entry CHARGE"); }
// void entry_publish(void) { ESP_LOGI("FSM", "Entry PUBLISH"); }
// void entry_log_bug(void) { ESP_LOGI("FSM", "Entry LOG_BUG"); }
// void entry_restart(void) { ESP_LOGI("FSM", "Entry RESTART"); }

// void fsm_init_entry_actions(void) {
// 	FsmEntryActions[STATE_BOOT] = entry_boot;
// 	FsmEntryActions[STATE_CONFIG] = entry_config;
// 	FsmEntryActions[STATE_OTA] = entry_ota;
// 	FsmEntryActions[STATE_INTERNET] = entry_internet;
// 	FsmEntryActions[STATE_MQTT] = entry_mqtt;
// 	FsmEntryActions[STATE_IDLE] = entry_idle;
// 	FsmEntryActions[STATE_CONFIRM] = entry_confirm;
// 	FsmEntryActions[STATE_CHARGE] = entry_charge;
// 	FsmEntryActions[STATE_PUBLISH] = entry_publish;
// 	FsmEntryActions[STATE_LOG_BUG] = entry_log_bug;
// 	FsmEntryActions[STATE_RESTART] = entry_restart;
// }

// void fsm_init_table(void) {
// 	// Ví dụ khởi tạo bảng FSM, cần chỉnh sửa theo logic thực tế
// 	FSM_Table[0] = (FsmTableEntry){STATE_BOOT, NULL, EVENT_DATA_VALID, STATE_INTERNET};
// 	FSM_Table[1] = (FsmTableEntry){STATE_BOOT, NULL, EVENT_DATA_INVALID, STATE_CONFIG};
// 	FSM_Table[2] = (FsmTableEntry){STATE_CONFIG, NULL, EVENT_CONFIG_COMPLETE, STATE_RESTART};
// 	FSM_Table[3] = (FsmTableEntry){STATE_INTERNET, NULL, EVENT_START_NET, STATE_INTERNET};
// 	FSM_Table[4] = (FsmTableEntry){STATE_INTERNET, NULL, EVENT_RETRY_NET, STATE_INTERNET};
// 	FSM_Table[5] = (FsmTableEntry){STATE_INTERNET, NULL, EVENT_INTERNET_COMPLETE, STATE_OTA};
// 	FSM_Table[6] = (FsmTableEntry){STATE_INTERNET, NULL, EVENT_INTERNET_FAILED, STATE_RESTART};
// 	FSM_Table[7] = (FsmTableEntry){STATE_OTA, NULL, EVENT_FLAG_OTA_VALID, STATE_OTA};
// 	FSM_Table[8] = (FsmTableEntry){STATE_OTA, NULL, EVENT_FLAG_OTA_UNVALID, STATE_MQTT};
// 	FSM_Table[9] = (FsmTableEntry){STATE_MQTT, NULL, EVENT_RETRY_MQTT, STATE_MQTT};
// 	FSM_Table[10] = (FsmTableEntry){STATE_MQTT, NULL, EVENT_MQTT_CONNECT_COMPLETE, STATE_IDLE};
// 	FSM_Table[11] = (FsmTableEntry){STATE_MQTT, NULL, EVENT_MQTT_CONNECT_FAILED, STATE_LOG_BUG};
// 	FSM_Table[12] = (FsmTableEntry){STATE_IDLE, NULL, EVENT_CHARGING_START, STATE_CONFIRM};
// 	FSM_Table[13] = (FsmTableEntry){STATE_CONFIRM, NULL, EVENT_CONFIRM_CHARGING_COMPLETE, STATE_CHARGE};
// 	FSM_Table[14] = (FsmTableEntry){STATE_IDLE, NULL, EVENT_RESET_CONFIG, STATE_BOOT};
// 	FSM_Table[15] = (FsmTableEntry){STATE_CHARGE, NULL, EVENT_CONTINUE_CHARGE, STATE_CHARGE};
// 	FSM_Table[16] = (FsmTableEntry){STATE_CHARGE, NULL, EVENT_CHARGING_STOP, STATE_PUBLISH};
// 	FSM_Table[17] = (FsmTableEntry){STATE_PUBLISH, NULL, EVENT_PUBLISH_COMPLETE, STATE_IDLE};
// 	FSM_Table[18] = (FsmTableEntry){STATE_LOG_BUG, NULL, EVENT_LOG_BUG_END, STATE_BOOT};
// 	FSM_Table[19] = (FsmTableEntry){STATE_IDLE, NULL, EVENT_FLAG_OTA_VALID, STATE_OTA};
// }

// void fsm_event_receiver(void *param) {
// 	fsm_init_table();
// 	fsm_init_entry_actions();
// 	FsmState current_state = STATE_BOOT;
// 	FsmEvent event;
// 	fsm_queue = xQueueCreate(10, sizeof(FsmEvent));
// 	if (fsm_queue == NULL) {
// 		ESP_LOGE("FSM", "Queue create failed");
// 		return;
// 	}
// 	while (1) {
// 		if (xQueueReceive(fsm_queue, &event, portMAX_DELAY) == pdPASS) {
// 			ESP_LOGI("FSM", "Received event: %d", event);
// 			fsm_handle_event(&current_state, event);
// 		}
// 	}
// }

// void fsm_handle_event(FsmState *current_state, FsmEvent event) {
// 	for (int i = 0; i < FSM_TABLE_SIZE; i++) {
// 		if (FSM_Table[i].from == *current_state && FSM_Table[i].event == event) {
// 			ESP_LOGI("FSM", "Transition: %d --(%d)--> %d", FSM_Table[i].from, event, FSM_Table[i].to);
// 			if (FSM_Table[i].action) {
// 				FSM_Table[i].action();
// 			}
// 			*current_state = FSM_Table[i].to;
// 			if (FsmEntryActions[*current_state]) {
// 				FsmEntryActions[*current_state]();
// 			}
// 			break;
// 		}
// 	}
// }
