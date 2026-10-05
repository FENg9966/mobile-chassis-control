# RT-Thread Studio工程配置

已检查现有工程：`D:\RT-ThreadStudio\workspace\H743_RTThread_Test`。
它目前只启用了 `uart3`，并把 `uart3` 作为MSH控制台；PWM框架和PWM1均未启用。
应用代码已经通过该工程头文件和GNU Arm工具链的 `-Wall -Wextra -Werror`
语法编译检查，但要生成可运行固件，还需完成下面的BSP配置。

## 一、开启RT-Thread组件

在 RT-Thread Settings 中开启：

- Device Drivers -> PWM device drivers；
- Serial device drivers；
- Pin device drivers。

保存后确认 `rtconfig.h` 中出现 `RT_USING_PWM`。

## 二、修改 `drivers/board.h`

保留UART3控制台，再添加UART2：

```c
#define BSP_USING_UART2
#define BSP_UART2_TX_PIN       "PA2"
#define BSP_UART2_RX_PIN       "PA3"

#define BSP_USING_UART3
#define BSP_UART3_TX_PIN       "PD8"
#define BSP_UART3_RX_PIN       "PD9"
```

在PWM区域添加：

```c
#define BSP_USING_PWM1
#define BSP_USING_PWM1_CH1
#define BSP_USING_PWM1_CH2
#define BSP_USING_PWM1_CH3
#define BSP_USING_PWM1_CH4
```

## 三、补充PWM1设备描述

在 `drivers/include/config/pwm_config.h` 中、`PWM2_CONFIG` 之前加入：

```c
#ifdef BSP_USING_PWM1
#ifndef PWM1_CONFIG
#define PWM1_CONFIG                             \
    {                                           \
       .tim_handle.Instance = TIM1,             \
       .name                = "pwm1",           \
       .channel             = 0                 \
    }
#endif
#endif
```

## 四、补充TIM1时钟和GPIO复用

在 `drivers/board.c` 末尾加入以下内容。如果工程中已经存在同名函数，合并
对应的 `TIM1` 分支，不要重复定义函数。

```c
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim_pwm)
{
    if (htim_pwm->Instance == TIM1)
    {
        __HAL_RCC_TIM1_CLK_ENABLE();
    }
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef gpio = {0};

    if (htim->Instance == TIM1)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        gpio.Pin = GPIO_PIN_9 | GPIO_PIN_11 |
                   GPIO_PIN_13 | GPIO_PIN_14;
        gpio.Mode = GPIO_MODE_AF_PP;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        gpio.Alternate = GPIO_AF1_TIM1;
        HAL_GPIO_Init(GPIOE, &gpio);
    }
}
```

## 五、加入应用代码

把整个 `D:\RT-thread代码` 目录复制到工程的
`applications\wheelchair_chassis`。在工程根目录的 `SConscript` 中加入：

```python
objs = objs + SConscript('applications/wheelchair_chassis/SConscript')
```

重新生成工程索引并编译。烧录后先执行：

```text
list_device
chassis_status
```

应看到 `uart2`、`uart3`、`pwm1`，且 `chassis_status` 能输出超声波状态。

## 六、首次上电限制

- 先不接24V电机动力线，只检查设备注册、GPIO和PWM波形；
- 急停PG4若尚未接常闭触点，代码会保持急停状态。仅架空调试时可临时把
  `CHASSIS_ESTOP_MONITOR_ENABLE` 改为0；
- 轮子架空后再接动力，屏幕速度先设10%；
- 确认24V锂电池满电电压不超过所用BTS7960模块的允许输入电压。

