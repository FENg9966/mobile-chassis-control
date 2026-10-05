#ifndef CHASSIS_PROTOCOL_H
#define CHASSIS_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHASSIS_PACKET_MAGIC          0xA55AU
#define CHASSIS_PROTOCOL_VERSION      1U
#define CHASSIS_WIRELESS_TIMEOUT_MS   250U

typedef enum {
    CHASSIS_MSG_COMMAND = 1,
    CHASSIS_MSG_ACK = 2,
    CHASSIS_MSG_TELEMETRY = 3,
} chassis_message_type_t;

typedef enum {
    CHASSIS_CMD_STOP = 0,
    CHASSIS_CMD_FORWARD = 1,
    CHASSIS_CMD_REVERSE = 2,
    CHASSIS_CMD_LEFT = 3,
    CHASSIS_CMD_RIGHT = 4,
} chassis_command_t;

enum {
    CHASSIS_STATUS_LINK_OK       = (1U << 0),
    CHASSIS_STATUS_ESTOP         = (1U << 1),
    CHASSIS_STATUS_OBSTACLE      = (1U << 2),
    CHASSIS_STATUS_DRIVER_FAULT  = (1U << 3),
    CHASSIS_STATUS_LOW_BATTERY   = (1U << 4),
};

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t sequence;
    uint8_t command;
    uint8_t speed_percent;
    int16_t left_rpm;
    int16_t right_rpm;
    uint16_t battery_mv;
    uint16_t status;
    uint16_t crc16;
} chassis_packet_t;

uint16_t chassis_crc16_ccitt(const void *data, size_t length);
void chassis_packet_finalize(chassis_packet_t *packet);
bool chassis_packet_is_valid(const chassis_packet_t *packet, size_t length);

#endif
