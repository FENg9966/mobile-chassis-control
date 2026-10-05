# 代码阅读导航

## 屏幕端

- `firmware/handheld/main/board.h`：SPI、I2C、背光与触控GPIO。
- `display/st7796.c`：屏幕寄存器初始化与显示参数。
- `display/display.c`：LVGL显示驱动注册、缓冲和刷屏回调。
- `hardware/touch.c`：FT6336读取及输入坐标处理。
- `display/lvgl_demo_ui.c`：方向按钮、停止按钮、速度滑条及命令更新。
- `wireless/chassis_protocol.c`：CRC与帧校验。
- `wireless/espnow_remote.c`：50ms指令心跳、ACK及遥测接收。

## 无线桥接端

- `firmware/chassis-bridge/main/main.c`：ESP-NOW与UART转发。
- `firmware/chassis-bridge/main/bridge_config.h`：UART GPIO、超时与通道配置。

## H743应用层

- `chassis_app.c`：初始化、遥测任务与MSH状态/停止命令。
- `chassis_control.c`：运动状态、失联与障碍停车逻辑。
- `motor_bts7960.c`：PWM输出、驱动使能与制动状态。
- `ultrasonic_hcsr04.c`：测距输入与结果管理。
- `chassis_link.c`：帧解析、CRC16和命令回调。
- `chassis_config.h`：20ms控制周期、4kHz PWM、超时与建议引脚。

引脚和参数是当前方案配置，不代表所有STM32H743开发板都可以直接照搬。源码存在不等于硬件功能已经测试通过。
