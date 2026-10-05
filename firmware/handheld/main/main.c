
/* INCLUDES */
#include "main.h"
#include "board.h"
#include "display/st7796.h"
#include "display/display.h"
#include "display/lvgl_demo_ui.h"
#include "hardware/touch.h"
#include "wireless/espnow_remote.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

/* VARIABLES */
static const char *TAG = "main";

static lv_disp_t *g_disp = NULL;

/* PRIVATE FUNCTIONS */
extern void example_lvgl_demo_ui(lv_disp_t *disp);

static void lvgl_task(void *param)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1) {
        lv_timer_handler();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ST7796 + FT6336U + LVGL Boot ===");
    ESP_LOGI(TAG, "LCD: %dx%d SPI, Touch: FT6336U I2C addr=0x%02X",
             LCD_H_RES, LCD_V_RES, FT6336_I2C_ADDR);

    /* 1. ST7796 LCD 初始化 (SPI + 背光 + 初始化序列) */
    st7796_displayInit();

    /* 2. LVGL 显示驱动注册 + 缓冲分配(PSRAM) + 示例UI */
    displayConfig();

    /* 从display.c里注册的全局disp在example_lvgl_demo_ui结束后可直接拿 */
    g_disp = lv_disp_get_default();

    /* 3. FT6336U 触摸初始化 + LVGL输入设备注册 */
    if (touch_init() == ESP_OK) {
        touch_lvgl_init(g_disp);
        ESP_LOGI(TAG, "Touch ready");
    } else {
        ESP_LOGW(TAG, "Touch init failed");
    }

    /* 4. 启动 LVGL 刷新任务（绑Core1，与WiFi解耦） */
    /* Handheld radio starts in STOP and transmits a command heartbeat at 20 Hz. */
    ESP_ERROR_CHECK(espnow_remote_init());

    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 10000, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "Init done, entering idle loop");
}
