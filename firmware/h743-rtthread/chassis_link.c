#include "chassis_link.h"
#include "chassis_config.h"

#include <rtdevice.h>
#include <string.h>

#define CHASSIS_PACKET_MAGIC        0xA55AU
#define CHASSIS_PROTOCOL_VERSION    1U
#define CHASSIS_MSG_COMMAND         1U
#define CHASSIS_MSG_TELEMETRY       3U

#pragma pack(push, 1)
typedef struct
{
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
#pragma pack(pop)

typedef char chassis_packet_size_must_be_18_bytes[
    (sizeof(chassis_packet_t) == 18U) ? 1 : -1];

static rt_device_t s_serial;
static struct rt_semaphore s_rx_sem;
static chassis_command_callback_t s_command_callback;
static volatile rt_tick_t s_last_valid_tick;
static volatile rt_bool_t s_online = RT_FALSE;
static uint16_t s_tx_sequence;

static uint16_t crc16_ccitt(const void *data, rt_size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint16_t crc = 0xFFFFU;
    rt_size_t i;
    uint8_t bit;

    for (i = 0; i < length; ++i)
    {
        crc ^= (uint16_t)bytes[i] << 8;
        for (bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static rt_bool_t packet_is_valid(const chassis_packet_t *packet)
{
    chassis_packet_t copy;
    uint16_t received_crc;

    if (packet->magic != CHASSIS_PACKET_MAGIC ||
        packet->version != CHASSIS_PROTOCOL_VERSION)
    {
        return RT_FALSE;
    }
    memcpy(&copy, packet, sizeof(copy));
    received_crc = copy.crc16;
    copy.crc16 = 0;
    return received_crc == crc16_ccitt(&copy, sizeof(copy));
}

static rt_err_t uart_rx_indicate(rt_device_t device, rt_size_t size)
{
    RT_UNUSED(device);
    RT_UNUSED(size);
    rt_sem_release(&s_rx_sem);
    return RT_EOK;
}

static void parse_byte(uint8_t byte)
{
    static uint8_t frame[sizeof(chassis_packet_t)];
    static rt_size_t used;
    chassis_packet_t packet;

    if (used == 0U && byte != (uint8_t)(CHASSIS_PACKET_MAGIC & 0xFFU))
    {
        return;
    }
    if (used == 1U && byte != (uint8_t)(CHASSIS_PACKET_MAGIC >> 8))
    {
        used = (byte == (uint8_t)(CHASSIS_PACKET_MAGIC & 0xFFU)) ? 1U : 0U;
        frame[0] = byte;
        return;
    }

    frame[used++] = byte;
    if (used < sizeof(frame))
    {
        return;
    }

    memcpy(&packet, frame, sizeof(packet));
    used = 0;
    if (!packet_is_valid(&packet) || packet.type != CHASSIS_MSG_COMMAND ||
        packet.command > CHASSIS_CMD_RIGHT || packet.speed_percent > 100U)
    {
        return;
    }

    s_last_valid_tick = rt_tick_get();
    s_online = RT_TRUE;
    if (s_command_callback != RT_NULL)
    {
        s_command_callback((chassis_command_t)packet.command,
                           packet.speed_percent);
    }
}

static void chassis_link_thread(void *parameter)
{
    uint8_t buffer[64];
    rt_size_t length;
    rt_size_t i;
    const rt_tick_t timeout_ticks =
        rt_tick_from_millisecond(CHASSIS_LINK_TIMEOUT_MS);
    RT_UNUSED(parameter);

    if (s_command_callback != RT_NULL)
    {
        s_command_callback(CHASSIS_CMD_STOP, 0U);
    }

    while (1)
    {
        rt_sem_take(&s_rx_sem, rt_tick_from_millisecond(20U));
        do
        {
            length = rt_device_read(s_serial, 0, buffer, sizeof(buffer));
            for (i = 0; i < length; ++i)
            {
                parse_byte(buffer[i]);
            }
        } while (length > 0U);

        if (s_online &&
            (rt_tick_get() - s_last_valid_tick) > timeout_ticks)
        {
            s_online = RT_FALSE;
            if (s_command_callback != RT_NULL)
            {
                s_command_callback(CHASSIS_CMD_STOP, 0U);
            }
        }
    }
}

int chassis_link_init(const char *serial_name,
                      chassis_command_callback_t command_callback)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    rt_thread_t thread;

    if (serial_name == RT_NULL || command_callback == RT_NULL)
    {
        return -RT_EINVAL;
    }

    s_serial = rt_device_find(serial_name);
    if (s_serial == RT_NULL)
    {
        rt_kprintf("[link] serial %s not found\n", serial_name);
        return -RT_ENOSYS;
    }

    config.baud_rate = BAUD_RATE_115200;
    config.data_bits = DATA_BITS_8;
    config.stop_bits = STOP_BITS_1;
    config.parity = PARITY_NONE;
    rt_device_control(s_serial, RT_DEVICE_CTRL_CONFIG, &config);

    rt_sem_init(&s_rx_sem, "chrx", 0, RT_IPC_FLAG_FIFO);
    rt_device_set_rx_indicate(s_serial, uart_rx_indicate);
    if (rt_device_open(s_serial, RT_DEVICE_FLAG_INT_RX) != RT_EOK)
    {
        return -RT_ERROR;
    }

    s_command_callback = command_callback;
    s_last_valid_tick = rt_tick_get();
    thread = rt_thread_create("ch_link", chassis_link_thread, RT_NULL,
                              2048, 9, 10);
    if (thread == RT_NULL)
    {
        return -RT_ENOMEM;
    }
    rt_thread_startup(thread);
    return RT_EOK;
}

int chassis_link_send_telemetry(int16_t left_rpm, int16_t right_rpm,
                                uint16_t battery_mv, uint16_t status)
{
    chassis_packet_t packet;
    rt_size_t written;

    if (s_serial == RT_NULL)
    {
        return -RT_ERROR;
    }

    memset(&packet, 0, sizeof(packet));
    packet.magic = CHASSIS_PACKET_MAGIC;
    packet.version = CHASSIS_PROTOCOL_VERSION;
    packet.type = CHASSIS_MSG_TELEMETRY;
    packet.sequence = ++s_tx_sequence;
    packet.left_rpm = left_rpm;
    packet.right_rpm = right_rpm;
    packet.battery_mv = battery_mv;
    packet.status = status | (s_online ? CHASSIS_STATUS_LINK_OK : 0U);
    packet.crc16 = crc16_ccitt(&packet, sizeof(packet));

    written = rt_device_write(s_serial, 0, &packet, sizeof(packet));
    return (written == sizeof(packet)) ? RT_EOK : -RT_ERROR;
}

rt_bool_t chassis_link_is_online(void)
{
    return s_online;
}
