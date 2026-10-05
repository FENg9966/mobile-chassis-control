#include "chassis_protocol.h"
#include <string.h>

uint16_t chassis_crc16_ccitt(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)bytes[i] << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

void chassis_packet_finalize(chassis_packet_t *packet)
{
    packet->magic = CHASSIS_PACKET_MAGIC;
    packet->version = CHASSIS_PROTOCOL_VERSION;
    packet->crc16 = 0;
    packet->crc16 = chassis_crc16_ccitt(packet, sizeof(*packet));
}

bool chassis_packet_is_valid(const chassis_packet_t *packet, size_t length)
{
    if (packet == NULL || length != sizeof(*packet) ||
        packet->magic != CHASSIS_PACKET_MAGIC ||
        packet->version != CHASSIS_PROTOCOL_VERSION) {
        return false;
    }
    chassis_packet_t copy;
    memcpy(&copy, packet, sizeof(copy));
    const uint16_t received = copy.crc16;
    copy.crc16 = 0;
    return received == chassis_crc16_ccitt(&copy, sizeof(copy));
}
