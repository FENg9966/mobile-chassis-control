#ifndef ULTRASONIC_HCSR04_H
#define ULTRASONIC_HCSR04_H

#include <rtthread.h>
#include <stdint.h>

int ultrasonic_hcsr04_init(void);
rt_bool_t ultrasonic_hcsr04_get(uint16_t *distance_mm);
rt_bool_t ultrasonic_hcsr04_is_obstacle(void);

#endif
