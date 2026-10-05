#ifndef CHASSIS_LINK_H
#define CHASSIS_LINK_H

#include <rtthread.h>
#include <stdint.h>
#include "chassis_types.h"

typedef void (*chassis_command_callback_t)(chassis_command_t command,
                                           uint8_t speed_percent);

int chassis_link_init(const char *serial_name,
                      chassis_command_callback_t command_callback);
int chassis_link_send_telemetry(int16_t left_rpm, int16_t right_rpm,
                                uint16_t battery_mv, uint16_t status);
rt_bool_t chassis_link_is_online(void);

#endif
