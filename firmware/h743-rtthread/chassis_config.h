#ifndef CHASSIS_CONFIG_H
#define CHASSIS_CONFIG_H

#include <board.h>

/* ESP32-S3 chassis bridge: UART 115200, 8N1. UART3 is kept for MSH console. */
#define CHASSIS_LINK_UART_NAME              "uart2"
#define CHASSIS_LINK_TIMEOUT_MS             300U

/*
 * Suggested STM32H743ZIT6 mapping. Verify against the actual development board.
 * TIM1_CH1..CH4 are PE9/PE11/PE13/PE14 on STM32H743ZIT6 AF1.
 */
#define CHASSIS_PWM_DEVICE_NAME              "pwm1"
#define CHASSIS_LEFT_RPWM_CHANNEL            1
#define CHASSIS_LEFT_LPWM_CHANNEL            2
#define CHASSIS_RIGHT_RPWM_CHANNEL           3
#define CHASSIS_RIGHT_LPWM_CHANNEL           4
#define CHASSIS_PWM_FREQUENCY_HZ             4000U
#define CHASSIS_PWM_PERIOD_NS                (1000000000U / CHASSIS_PWM_FREQUENCY_HZ)

/* Set one side to 1 if that wheel runs backward during a FORWARD command. */
#define CHASSIS_LEFT_MOTOR_INVERT            0
#define CHASSIS_RIGHT_MOTOR_INVERT           0

/* R_EN and L_EN of each BTS7960 are tied together on the interface board. */
#define CHASSIS_LEFT_ENABLE_PIN              GET_PIN(G, 0)
#define CHASSIS_RIGHT_ENABLE_PIN             GET_PIN(G, 1)
#define CHASSIS_DRIVER_ENABLE_LEVEL          PIN_HIGH

/* High level releases the brake through an external MOS switch. */
#define CHASSIS_LEFT_BRAKE_PIN               GET_PIN(G, 2)
#define CHASSIS_RIGHT_BRAKE_PIN              GET_PIN(G, 3)
#define CHASSIS_BRAKE_RELEASE_LEVEL          PIN_HIGH

/*
 * Fail-safe emergency-stop monitor input:
 * normal NC auxiliary contact connects the pin to GND;
 * pressing the emergency stop opens it and the pull-up produces PIN_HIGH.
 */
#define CHASSIS_ESTOP_MONITOR_ENABLE         1
#define CHASSIS_ESTOP_PIN                    GET_PIN(G, 4)
#define CHASSIS_ESTOP_ACTIVE_LEVEL           PIN_HIGH

/* HC-SR04. ECHO must pass through a 5 V -> 3.3 V divider before the MCU. */
#define CHASSIS_HCSR04_TRIG_PIN              GET_PIN(G, 5)
#define CHASSIS_HCSR04_ECHO_PIN              GET_PIN(G, 6)
#define CHASSIS_HCSR04_PERIOD_MS             100U
#define CHASSIS_HCSR04_TIMEOUT_US            30000U
#define CHASSIS_OBSTACLE_STOP_MM             600U
#define CHASSIS_OBSTACLE_RELEASE_MM          750U

/* Conservative first-test limits. Increase only after current/temperature tests. */
#define CHASSIS_MAX_DUTY_PERMILLE            300
#define CHASSIS_RAMP_STEP_PER_20MS           3
#define CHASSIS_CONTROL_PERIOD_MS            20U
#define CHASSIS_BRAKE_RELEASE_DELAY_MS       300U
#define CHASSIS_BRAKE_ENGAGE_DELAY_MS        200U

/* 0: gentle one-wheel turn; 1: wheels run in opposite directions. */
#define CHASSIS_TURN_IN_PLACE                0

#endif
