
#include "touch.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* FT6336U 寄存器定义 */
#define FT6336_REG_DEV_MODE       0x00
#define FT6336_REG_GEST_ID        0x01
#define FT6336_REG_TD_STATUS      0x02
#define FT6336_REG_P1_XH          0x03
#define FT6336_REG_P1_XL          0x04
#define FT6336_REG_P1_YH          0x05
#define FT6336_REG_P1_YL          0x06
#define FT6336_REG_P2_XH          0x09
#define FT6336_REG_P2_XL          0x0A
#define FT6336_REG_P2_YH          0x0B
#define FT6336_REG_P2_YL          0x0C
#define FT6336_REG_THGROUP        0x80
#define FT6336_REG_THDIFF         0x85
#define FT6336_REG_CTRL           0x86
#define FT6336_REG_PERIODACTIVE   0x88
#define FT6336_REG_PERIODMONITOR  0x89
#define FT6336_REG_LIB_VERSION_H  0xA1
#define FT6336_REG_LIB_VERSION_L  0xA2
#define FT6336_REG_CIPHER         0xA3
#define FT6336_REG_G_MODE         0xA4
#define FT6336_REG_PWR_MODE       0xA5
#define FT6336_REG_FIRMID         0xA6
#define FT6336_REG_FOCALTECH_ID   0xA8
#define FT6336_REG_RELEASE_CODE   0xAF
#define FT6336_REG_STATE          0xBC

#define I2C_MASTER_TIMEOUT_MS    1000
#define I2C_MASTER_FREQ_HZ       400000

static const char *TAG = "touch";

esp_err_t touch_init(void)
{
    ESP_LOGI(TAG, "Init I2C host=%d SCL=%d SDA=%d FT6336U addr=0x%02X",
             I2C_HOST, I2C_PIN_SCL, I2C_PIN_SDA, FT6336_I2C_ADDR);

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_PIN_SDA,
        .scl_io_num = I2C_PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_HOST, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_HOST, conf.mode, 0, 0, 0));

    /* 触摸复位脚: 拉低 10ms -> 拉高等待 */
    if (TP_PIN_RST >= 0) {
        gpio_config_t rst_cfg = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << TP_PIN_RST,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
        };
        gpio_config(&rst_cfg);
        gpio_set_level(TP_PIN_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(TP_PIN_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    /* INT引脚：可选输入，此处不作中断用（轮询模式足够LVGL）*/
    if (TP_PIN_INT >= 0) {
        gpio_config_t int_cfg = {
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = 1ULL << TP_PIN_INT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
        };
        gpio_config(&int_cfg);
    }

    /* 读取芯片ID验证 */
    uint8_t chip_id = 0;
    uint8_t reg = FT6336_REG_FOCALTECH_ID;
    i2c_master_write_read_device(I2C_HOST, FT6336_I2C_ADDR,
                                 &reg, 1, &chip_id, 1,
                                 pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    ESP_LOGI(TAG, "FT6336U FOCALTECH_ID=0x%02X (expected 0x11)", chip_id);

    /* 设置工作模式: 轮询报告 / 触发模式均可，0xA4=0x00 为轮询(上电默认即可) */
    return ESP_OK;
}

bool touch_read(touch_point_t *tp)
{
    uint8_t reg = FT6336_REG_TD_STATUS;
    uint8_t rbuf[7]; /* TD_STATUS + P1_XH/XL/YH/YL + 2 bytes padding */

    if (i2c_master_write_read_device(I2C_HOST, FT6336_I2C_ADDR,
                                     &reg, 1, rbuf, 7,
                                     pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS)) != ESP_OK) {
        tp->pressed = false;
        return false;
    }

    uint8_t td_status = rbuf[0] & 0x0F;
    if (td_status == 0) {
        tp->pressed = false;
        return true;
    }

    uint16_t x = ((uint16_t)(rbuf[1] & 0x0F) << 8) | rbuf[2];
    uint16_t y = ((uint16_t)(rbuf[3] & 0x0F) << 8) | rbuf[4];

    /* 坐标范围裁剪 */
    if (x >= LCD_H_RES) x = LCD_H_RES - 1;
    if (y >= LCD_V_RES) y = LCD_V_RES - 1;

    tp->x = x;
    tp->y = y;
    tp->pressed = true;
    return true;
}

/* LVGL 输入设备回调 */
static void touchpad_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    touch_point_t tp;
    if (touch_read(&tp)) {
        if (tp.pressed) {
            data->point.x = tp.x;
            data->point.y = tp.y;
            data->state = LV_INDEV_STATE_PR;
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void touch_lvgl_init(lv_disp_t *disp)
{
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read_cb;
    indev_drv.disp = disp;
    lv_indev_drv_register(&indev_drv);
    ESP_LOGI(TAG, "LVGL touch input registered");
}
