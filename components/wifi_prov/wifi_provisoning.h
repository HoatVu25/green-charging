#pragma once

#include <stdbool.h>

#define PROV_SERVICE_NAME_PREFIX    "PROV_"

#define APP_PROOF_OF_POSSESSION     "abcd1234"

void wifi_provisioning_init(void);
bool wifi_is_provisioned(void);
