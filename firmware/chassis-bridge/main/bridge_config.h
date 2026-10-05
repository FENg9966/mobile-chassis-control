#ifndef BRIDGE_CONFIG_H
#define BRIDGE_CONFIG_H

#include "driver/gpio.h"
#include "driver/uart.h"

/* Change these two pins if they conflict with your second ESP32-S3 board. */
#define BRIDGE_UART_PORT       UART_NUM_1
#define BRIDGE_UART_TX_PIN     GPIO_NUM_17
#define BRIDGE_UART_RX_PIN     GPIO_NUM_18
#define BRIDGE_UART_BAUD       115200
#define BRIDGE_ESPNOW_CHANNEL  6

#endif
