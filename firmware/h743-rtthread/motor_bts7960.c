#include "motor_bts7960.h"
#include "chassis_config.h"

#include <rtdevice.h>

static struct rt_device_pwm *s_pwm;

static int16_t clamp_permille(int16_t value)
{
    if (value > 1000)
    {
        return 1000;
    }
    if (value < -1000)
    {
        return -1000;
    }
    return value;
}

static rt_uint32_t pulse_from_permille(int16_t value)
{
    rt_uint32_t magnitude = (value < 0) ? (rt_uint32_t)(-value)
                                        : (rt_uint32_t)value;
    return (CHASSIS_PWM_PERIOD_NS * magnitude) / 1000U;
}

static void set_channel(int channel, rt_uint32_t pulse_ns)
{
    rt_pwm_set(s_pwm, channel, CHASSIS_PWM_PERIOD_NS, pulse_ns);
}

static void set_one_motor(int rpwm_channel, int lpwm_channel,
                          int16_t command)
{
    rt_uint32_t pulse;
    command = clamp_permille(command);
    pulse = pulse_from_permille(command);

    /* First clear both inputs so RPWM and LPWM are never active together. */
    set_channel(rpwm_channel, 0U);
    set_channel(lpwm_channel, 0U);

    if (command > 0)
    {
        set_channel(rpwm_channel, pulse);
    }
    else if (command < 0)
    {
        set_channel(lpwm_channel, pulse);
    }
}

int motor_bts7960_init(void)
{
    s_pwm = (struct rt_device_pwm *)rt_device_find(CHASSIS_PWM_DEVICE_NAME);
    if (s_pwm == RT_NULL)
    {
        rt_kprintf("[motor] PWM device %s not found\n",
                   CHASSIS_PWM_DEVICE_NAME);
        return -RT_ENOSYS;
    }

    rt_pin_mode(CHASSIS_LEFT_ENABLE_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(CHASSIS_RIGHT_ENABLE_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(CHASSIS_LEFT_BRAKE_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(CHASSIS_RIGHT_BRAKE_PIN, PIN_MODE_OUTPUT);

    motor_bts7960_set_enable(RT_FALSE);
    motor_brake_set_released(RT_FALSE);

    set_channel(CHASSIS_LEFT_RPWM_CHANNEL, 0U);
    set_channel(CHASSIS_LEFT_LPWM_CHANNEL, 0U);
    set_channel(CHASSIS_RIGHT_RPWM_CHANNEL, 0U);
    set_channel(CHASSIS_RIGHT_LPWM_CHANNEL, 0U);

    rt_pwm_enable(s_pwm, CHASSIS_LEFT_RPWM_CHANNEL);
    rt_pwm_enable(s_pwm, CHASSIS_LEFT_LPWM_CHANNEL);
    rt_pwm_enable(s_pwm, CHASSIS_RIGHT_RPWM_CHANNEL);
    rt_pwm_enable(s_pwm, CHASSIS_RIGHT_LPWM_CHANNEL);
    return RT_EOK;
}

void motor_bts7960_apply(int16_t left_permille, int16_t right_permille)
{
    if (s_pwm == RT_NULL)
    {
        return;
    }
#if CHASSIS_LEFT_MOTOR_INVERT
    left_permille = -left_permille;
#endif
#if CHASSIS_RIGHT_MOTOR_INVERT
    right_permille = -right_permille;
#endif
    set_one_motor(CHASSIS_LEFT_RPWM_CHANNEL, CHASSIS_LEFT_LPWM_CHANNEL,
                  left_permille);
    set_one_motor(CHASSIS_RIGHT_RPWM_CHANNEL, CHASSIS_RIGHT_LPWM_CHANNEL,
                  right_permille);
}

void motor_bts7960_set_enable(rt_bool_t enabled)
{
    int level = enabled ? CHASSIS_DRIVER_ENABLE_LEVEL
                        : !CHASSIS_DRIVER_ENABLE_LEVEL;
    rt_pin_write(CHASSIS_LEFT_ENABLE_PIN, level);
    rt_pin_write(CHASSIS_RIGHT_ENABLE_PIN, level);
}

void motor_brake_set_released(rt_bool_t released)
{
    int level = released ? CHASSIS_BRAKE_RELEASE_LEVEL
                         : !CHASSIS_BRAKE_RELEASE_LEVEL;
    rt_pin_write(CHASSIS_LEFT_BRAKE_PIN, level);
    rt_pin_write(CHASSIS_RIGHT_BRAKE_PIN, level);
}

void motor_bts7960_emergency_stop(void)
{
    motor_bts7960_apply(0, 0);
    motor_bts7960_set_enable(RT_FALSE);
    motor_brake_set_released(RT_FALSE);
}
