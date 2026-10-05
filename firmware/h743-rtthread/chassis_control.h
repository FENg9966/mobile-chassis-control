#ifndef CHASSIS_CONTROL_H
#define CHASSIS_CONTROL_H

#include <rtthread.h>
#include <stdint.h>
#include "chassis_types.h"

typedef enum
{
    CHASSIS_STATE_STOPPED = 0,
    CHASSIS_STATE_RELEASING_BRAKE,
    CHASSIS_STATE_RUNNING,
    CHASSIS_STATE_STOPPING,
    CHASSIS_STATE_FAULT,
} chassis_state_t;

int chassis_control_init(void);
void chassis_control_set_remote(chassis_command_t command,
                                uint8_t speed_percent);
uint16_t chassis_control_get_status(void);
chassis_state_t chassis_control_get_state(void);
void chassis_control_force_stop(void);

#endif
