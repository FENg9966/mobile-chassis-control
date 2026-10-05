#ifndef ESPNOW_REMOTE_H
#define ESPNOW_REMOTE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "chassis_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t espnow_remote_init(void);
void espnow_remote_set_command(chassis_command_t command, uint8_t speed_percent);
void espnow_remote_force_stop(void);
bool espnow_remote_is_linked(void);
bool espnow_remote_get_telemetry(chassis_packet_t *telemetry);

#ifdef __cplusplus
}
#endif

#endif
