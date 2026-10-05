#ifndef MOTOR_BTS7960_H
#define MOTOR_BTS7960_H

#include <rtthread.h>
#include <stdint.h>

int motor_bts7960_init(void);
void motor_bts7960_apply(int16_t left_permille, int16_t right_permille);
void motor_bts7960_set_enable(rt_bool_t enabled);
void motor_brake_set_released(rt_bool_t released);
void motor_bts7960_emergency_stop(void);

#endif
