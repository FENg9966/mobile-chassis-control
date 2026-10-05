#include "chassis_control.h"
#include "chassis_config.h"
#include "chassis_link.h"
#include "motor_bts7960.h"
#include "ultrasonic_hcsr04.h"

#include <rtdevice.h>

static struct rt_mutex s_command_lock;
static chassis_command_t s_requested_command = CHASSIS_CMD_STOP;
static uint8_t s_requested_speed;
static volatile chassis_state_t s_state = CHASSIS_STATE_STOPPED;
static volatile uint16_t s_status;
static int16_t s_left_current;
static int16_t s_right_current;
static rt_tick_t s_state_tick;
static rt_bool_t s_obstacle_latched;
static rt_bool_t s_reversal_latched;

static rt_bool_t estop_is_active(void)
{
#if CHASSIS_ESTOP_MONITOR_ENABLE
    return rt_pin_read(CHASSIS_ESTOP_PIN) == CHASSIS_ESTOP_ACTIVE_LEVEL;
#else
    return RT_FALSE;
#endif
}

static int16_t ramp_toward(int16_t current, int16_t target)
{
    if (current < target)
    {
        current += CHASSIS_RAMP_STEP_PER_20MS;
        return (current > target) ? target : current;
    }
    if (current > target)
    {
        current -= CHASSIS_RAMP_STEP_PER_20MS;
        return (current < target) ? target : current;
    }
    return current;
}

static rt_bool_t opposite_sign(int16_t current, int16_t target)
{
    return ((current > 0 && target < 0) ||
            (current < 0 && target > 0));
}

static void command_to_targets(chassis_command_t command,
                               uint8_t speed_percent,
                               int16_t *left, int16_t *right)
{
    int16_t value = (int16_t)(((int32_t)CHASSIS_MAX_DUTY_PERMILLE *
                               speed_percent) / 100);
    *left = 0;
    *right = 0;

    switch (command)
    {
    case CHASSIS_CMD_FORWARD:
        *left = value;
        *right = value;
        break;
    case CHASSIS_CMD_REVERSE:
        *left = -value;
        *right = -value;
        break;
    case CHASSIS_CMD_LEFT:
#if CHASSIS_TURN_IN_PLACE
        *left = -value;
#else
        *left = 0;
#endif
        *right = value;
        break;
    case CHASSIS_CMD_RIGHT:
        *left = value;
#if CHASSIS_TURN_IN_PLACE
        *right = -value;
#else
        *right = 0;
#endif
        break;
    case CHASSIS_CMD_STOP:
    default:
        break;
    }
}

static void hard_stop(chassis_state_t new_state)
{
    s_left_current = 0;
    s_right_current = 0;
    motor_bts7960_emergency_stop();
    s_state = new_state;
    s_state_tick = rt_tick_get();
}

static void chassis_control_thread(void *parameter)
{
    chassis_command_t command;
    uint8_t speed;
    int16_t left_target;
    int16_t right_target;
    rt_bool_t obstacle;
    rt_bool_t blocked;
    rt_tick_t now;
    RT_UNUSED(parameter);

    hard_stop(CHASSIS_STATE_STOPPED);

    while (1)
    {
        now = rt_tick_get();
        obstacle = ultrasonic_hcsr04_is_obstacle();

        rt_mutex_take(&s_command_lock, RT_WAITING_FOREVER);
        command = s_requested_command;
        speed = s_requested_speed;
        rt_mutex_release(&s_command_lock);

        s_status = 0U;
        if (chassis_link_is_online())
        {
            s_status |= CHASSIS_STATUS_LINK_OK;
        }
        if (estop_is_active())
        {
            s_status |= CHASSIS_STATUS_ESTOP;
            s_reversal_latched = RT_FALSE;
            hard_stop(CHASSIS_STATE_FAULT);
            rt_thread_mdelay(CHASSIS_CONTROL_PERIOD_MS);
            continue;
        }
        if (!chassis_link_is_online())
        {
            s_reversal_latched = RT_FALSE;
            hard_stop(CHASSIS_STATE_STOPPED);
            rt_thread_mdelay(CHASSIS_CONTROL_PERIOD_MS);
            continue;
        }

        blocked = obstacle && (command == CHASSIS_CMD_FORWARD ||
                               command == CHASSIS_CMD_LEFT ||
                               command == CHASSIS_CMD_RIGHT);
        if (blocked)
        {
            s_obstacle_latched = RT_TRUE;
        }
        if (command == CHASSIS_CMD_STOP)
        {
            s_obstacle_latched = RT_FALSE;
            s_reversal_latched = RT_FALSE;
        }
        if (blocked || s_obstacle_latched)
        {
            command = CHASSIS_CMD_STOP;
            speed = 0U;
        }
        if (obstacle || s_obstacle_latched)
        {
            s_status |= CHASSIS_STATUS_OBSTACLE;
        }

        command_to_targets(command, speed, &left_target, &right_target);

        /* Any direction reversal first completes a normal stop cycle. */
        if (!s_reversal_latched &&
            (opposite_sign(s_left_current, left_target) ||
             opposite_sign(s_right_current, right_target)))
        {
            s_reversal_latched = RT_TRUE;
        }
        if (s_reversal_latched)
        {
            left_target = 0;
            right_target = 0;
        }

        switch (s_state)
        {
        case CHASSIS_STATE_FAULT:
            hard_stop(CHASSIS_STATE_STOPPED);
            break;

        case CHASSIS_STATE_STOPPED:
            if (left_target != 0 || right_target != 0)
            {
                motor_bts7960_set_enable(RT_FALSE);
                motor_brake_set_released(RT_TRUE);
                s_state = CHASSIS_STATE_RELEASING_BRAKE;
                s_state_tick = now;
            }
            break;

        case CHASSIS_STATE_RELEASING_BRAKE:
            if (left_target == 0 && right_target == 0)
            {
                motor_brake_set_released(RT_FALSE);
                s_state = CHASSIS_STATE_STOPPED;
            }
            else if ((now - s_state_tick) >=
                     rt_tick_from_millisecond(CHASSIS_BRAKE_RELEASE_DELAY_MS))
            {
                motor_bts7960_set_enable(RT_TRUE);
                s_state = CHASSIS_STATE_RUNNING;
            }
            break;

        case CHASSIS_STATE_RUNNING:
            s_left_current = ramp_toward(s_left_current, left_target);
            s_right_current = ramp_toward(s_right_current, right_target);
            motor_bts7960_apply(s_left_current, s_right_current);
            if (left_target == 0 && right_target == 0 &&
                s_left_current == 0 && s_right_current == 0)
            {
                motor_bts7960_set_enable(RT_FALSE);
                s_state = CHASSIS_STATE_STOPPING;
                s_state_tick = now;
            }
            break;

        case CHASSIS_STATE_STOPPING:
            if (left_target != 0 || right_target != 0)
            {
                motor_bts7960_set_enable(RT_TRUE);
                s_state = CHASSIS_STATE_RUNNING;
            }
            else if ((now - s_state_tick) >=
                     rt_tick_from_millisecond(CHASSIS_BRAKE_ENGAGE_DELAY_MS))
            {
                motor_brake_set_released(RT_FALSE);
                s_state = CHASSIS_STATE_STOPPED;
                s_reversal_latched = RT_FALSE;
            }
            break;

        default:
            hard_stop(CHASSIS_STATE_FAULT);
            break;
        }

        rt_thread_mdelay(CHASSIS_CONTROL_PERIOD_MS);
    }
}

int chassis_control_init(void)
{
    rt_thread_t thread;
    rt_mutex_init(&s_command_lock, "cmdlock", RT_IPC_FLAG_PRIO);

#if CHASSIS_ESTOP_MONITOR_ENABLE
    rt_pin_mode(CHASSIS_ESTOP_PIN, PIN_MODE_INPUT_PULLUP);
#endif

    thread = rt_thread_create("ch_ctrl", chassis_control_thread, RT_NULL,
                              2048, 8, 10);
    if (thread == RT_NULL)
    {
        return -RT_ENOMEM;
    }
    rt_thread_startup(thread);
    return RT_EOK;
}

void chassis_control_set_remote(chassis_command_t command,
                                uint8_t speed_percent)
{
    if (command > CHASSIS_CMD_RIGHT || speed_percent > 100U)
    {
        command = CHASSIS_CMD_STOP;
        speed_percent = 0U;
    }
    if (command == CHASSIS_CMD_STOP)
    {
        speed_percent = 0U;
    }

    rt_mutex_take(&s_command_lock, RT_WAITING_FOREVER);
    s_requested_command = command;
    s_requested_speed = speed_percent;
    rt_mutex_release(&s_command_lock);
}

uint16_t chassis_control_get_status(void)
{
    return s_status;
}

chassis_state_t chassis_control_get_state(void)
{
    return s_state;
}

void chassis_control_force_stop(void)
{
    chassis_control_set_remote(CHASSIS_CMD_STOP, 0U);
    hard_stop(CHASSIS_STATE_STOPPED);
}
