#include "ultrasonic_hcsr04.h"
#include "chassis_config.h"

#include <rtdevice.h>

static struct rt_mutex s_lock;
static uint16_t s_distance_mm;
static rt_bool_t s_valid = RT_FALSE;
static rt_bool_t s_obstacle = RT_FALSE;

static void dwt_counter_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t elapsed_us(uint32_t start_cycles)
{
    uint32_t delta = DWT->CYCCNT - start_cycles;
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    if (cycles_per_us == 0U)
    {
        cycles_per_us = 1U;
    }
    return delta / cycles_per_us;
}

static rt_bool_t wait_pin_level(int level, uint32_t timeout_us)
{
    uint32_t started = DWT->CYCCNT;
    while (rt_pin_read(CHASSIS_HCSR04_ECHO_PIN) != level)
    {
        if (elapsed_us(started) >= timeout_us)
        {
            return RT_FALSE;
        }
    }
    return RT_TRUE;
}

static rt_bool_t measure_once(uint16_t *distance_mm)
{
    uint32_t echo_started;
    uint32_t duration_us;
    uint32_t distance;

    rt_pin_write(CHASSIS_HCSR04_TRIG_PIN, PIN_LOW);
    rt_hw_us_delay(3U);
    rt_pin_write(CHASSIS_HCSR04_TRIG_PIN, PIN_HIGH);
    rt_hw_us_delay(10U);
    rt_pin_write(CHASSIS_HCSR04_TRIG_PIN, PIN_LOW);

    if (!wait_pin_level(PIN_HIGH, CHASSIS_HCSR04_TIMEOUT_US))
    {
        return RT_FALSE;
    }

    echo_started = DWT->CYCCNT;
    if (!wait_pin_level(PIN_LOW, CHASSIS_HCSR04_TIMEOUT_US))
    {
        return RT_FALSE;
    }

    duration_us = elapsed_us(echo_started);
    distance = (duration_us * 343U + 1000U) / 2000U;
    if (distance < 20U || distance > 5000U)
    {
        return RT_FALSE;
    }

    *distance_mm = (uint16_t)distance;
    return RT_TRUE;
}

static uint16_t median3(uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t temp;
    if (a > b) { temp = a; a = b; b = temp; }
    if (b > c) { temp = b; b = c; c = temp; }
    if (a > b) { temp = a; a = b; b = temp; }
    return b;
}

static void ultrasonic_thread(void *parameter)
{
    uint16_t samples[3] = {0U, 0U, 0U};
    uint8_t sample_count = 0U;
    uint8_t invalid_count = 0U;
    uint16_t raw;
    uint16_t filtered;
    RT_UNUSED(parameter);

    while (1)
    {
        if (measure_once(&raw))
        {
            samples[sample_count % 3U] = raw;
            ++sample_count;
            invalid_count = 0U;
            filtered = (sample_count >= 3U)
                       ? median3(samples[0], samples[1], samples[2])
                       : raw;

            rt_mutex_take(&s_lock, RT_WAITING_FOREVER);
            s_distance_mm = filtered;
            s_valid = RT_TRUE;
            if (!s_obstacle && filtered <= CHASSIS_OBSTACLE_STOP_MM)
            {
                s_obstacle = RT_TRUE;
            }
            else if (s_obstacle && filtered >= CHASSIS_OBSTACLE_RELEASE_MM)
            {
                s_obstacle = RT_FALSE;
            }
            rt_mutex_release(&s_lock);
        }
        else
        {
            if (++invalid_count >= 3U)
            {
                rt_mutex_take(&s_lock, RT_WAITING_FOREVER);
                s_valid = RT_FALSE;
                rt_mutex_release(&s_lock);
            }
        }

        rt_thread_mdelay(CHASSIS_HCSR04_PERIOD_MS);
    }
}

int ultrasonic_hcsr04_init(void)
{
    rt_thread_t thread;
    rt_mutex_init(&s_lock, "uslock", RT_IPC_FLAG_PRIO);
    rt_pin_mode(CHASSIS_HCSR04_TRIG_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(CHASSIS_HCSR04_TRIG_PIN, PIN_LOW);
    rt_pin_mode(CHASSIS_HCSR04_ECHO_PIN, PIN_MODE_INPUT);
    dwt_counter_init();

    thread = rt_thread_create("hcsr04", ultrasonic_thread, RT_NULL,
                              1536, 18, 10);
    if (thread == RT_NULL)
    {
        return -RT_ENOMEM;
    }
    rt_thread_startup(thread);
    return RT_EOK;
}

rt_bool_t ultrasonic_hcsr04_get(uint16_t *distance_mm)
{
    rt_bool_t valid;
    if (distance_mm == RT_NULL)
    {
        return RT_FALSE;
    }
    rt_mutex_take(&s_lock, RT_WAITING_FOREVER);
    valid = s_valid;
    *distance_mm = s_distance_mm;
    rt_mutex_release(&s_lock);
    return valid;
}

rt_bool_t ultrasonic_hcsr04_is_obstacle(void)
{
    rt_bool_t obstacle;
    rt_mutex_take(&s_lock, RT_WAITING_FOREVER);
    obstacle = s_valid && s_obstacle;
    rt_mutex_release(&s_lock);
    return obstacle;
}
