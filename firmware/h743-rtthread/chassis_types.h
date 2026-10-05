#ifndef CHASSIS_TYPES_H
#define CHASSIS_TYPES_H

#include <stdint.h>

typedef enum
{
    CHASSIS_CMD_STOP = 0,
    CHASSIS_CMD_FORWARD = 1,
    CHASSIS_CMD_REVERSE = 2,
    CHASSIS_CMD_LEFT = 3,
    CHASSIS_CMD_RIGHT = 4,
} chassis_command_t;

enum
{
    CHASSIS_STATUS_LINK_OK       = (1U << 0),
    CHASSIS_STATUS_ESTOP         = (1U << 1),
    CHASSIS_STATUS_OBSTACLE      = (1U << 2),
    CHASSIS_STATUS_DRIVER_FAULT  = (1U << 3),
    CHASSIS_STATUS_LOW_BATTERY   = (1U << 4),
};

#endif
