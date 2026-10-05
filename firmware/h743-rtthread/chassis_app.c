#include <rtthread.h>
#include <rtdevice.h>

#include "chassis_config.h"
#include "chassis_control.h"
#include "chassis_link.h"
#include "motor_bts7960.h"
#include "ultrasonic_hcsr04.h"

static void remote_command_received(chassis_command_t command,
                                    uint8_t speed_percent)
{
    chassis_control_set_remote(command, speed_percent);
}

static void telemetry_thread(void *parameter)
{
    RT_UNUSED(parameter);
    while (1)
    {
        /* Encoders and battery ADC are not fitted yet, so these values are 0. */
        chassis_link_send_telemetry(0, 0, 0,
                                    chassis_control_get_status());
        rt_thread_mdelay(200U);
    }
}

static int chassis_app_init(void)
{
    rt_thread_t thread;
    int result;

    result = motor_bts7960_init();
    if (result != RT_EOK)
    {
        return result;
    }

    result = ultrasonic_hcsr04_init();
    if (result != RT_EOK)
    {
        motor_bts7960_emergency_stop();
        return result;
    }

    result = chassis_control_init();
    if (result != RT_EOK)
    {
        motor_bts7960_emergency_stop();
        return result;
    }

    result = chassis_link_init(CHASSIS_LINK_UART_NAME,
                               remote_command_received);
    if (result != RT_EOK)
    {
        motor_bts7960_emergency_stop();
        return result;
    }

    thread = rt_thread_create("ch_tlm", telemetry_thread, RT_NULL,
                              1536, 20, 10);
    if (thread == RT_NULL)
    {
        motor_bts7960_emergency_stop();
        return -RT_ENOMEM;
    }
    rt_thread_startup(thread);
    rt_kprintf("[chassis] application started in safe-stop state\n");
    return RT_EOK;
}
INIT_APP_EXPORT(chassis_app_init);

#ifdef RT_USING_FINSH
#include <finsh.h>

static void chassis_status_cmd(void)
{
    uint16_t distance = 0U;
    rt_bool_t valid = ultrasonic_hcsr04_get(&distance);
    rt_kprintf("state=%d link=%d status=0x%04x ultrasonic=%s",
               chassis_control_get_state(), chassis_link_is_online(),
               chassis_control_get_status(), valid ? "OK" : "INVALID");
    if (valid)
    {
        rt_kprintf(" distance=%u mm", distance);
    }
    rt_kprintf("\n");
}
MSH_CMD_EXPORT_ALIAS(chassis_status_cmd, chassis_status,
                     show chassis status and ultrasonic distance);

static void chassis_stop_cmd(void)
{
    chassis_control_force_stop();
    rt_kprintf("chassis forced to safe stop\n");
}
MSH_CMD_EXPORT_ALIAS(chassis_stop_cmd, chassis_stop,
                     immediately stop motors and engage brakes);
#endif
