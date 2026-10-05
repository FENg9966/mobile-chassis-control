
#include "st7796.h"
#include "display.h"

static const char *TAG = "st7796";

esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_panel_io_handle_t io_handle_global = NULL;

static void st7796_send_init_cmds(esp_lcd_panel_io_handle_t io)
{
    /* ST7796 关键寄存器配置（320x480 竖屏, RGB565） */
    /* 软件复位 */
    esp_lcd_panel_io_tx_param(io, 0x01, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* 退出睡眠模式 */
    esp_lcd_panel_io_tx_param(io, 0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* 像素格式 RGB565 = 0x55 (MCU 16bit + RGB 16bit) */
    uint8_t pixfmt = 0x55;
    esp_lcd_panel_io_tx_param(io, 0x3A, &pixfmt, 1);

    /* 显示倒相（根据IPS实际效果调整） */
    esp_lcd_panel_io_tx_param(io, 0x21, NULL, 0);

    /* 正常显示模式开 */
    esp_lcd_panel_io_tx_param(io, 0x13, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 列地址设置 CASET: X1=0, X2=319 (320列) */
    uint8_t caset[] = { 0x00, 0x00, 0x01, 0x3F }; /* 0x0000 ~ 0x013F = 0~319 */
    esp_lcd_panel_io_tx_param(io, 0x2A, caset, 4);

    /* 页地址设置 RASET: Y1=0, Y2=479 (480行) */
    uint8_t raset[] = { 0x00, 0x00, 0x01, 0xDF }; /* 0x0000 ~ 0x01DF = 0~479 */
    esp_lcd_panel_io_tx_param(io, 0x2B, raset, 4);

    vTaskDelay(pdMS_TO_TICKS(20));

    /* 显示开 */
    esp_lcd_panel_io_tx_param(io, 0x29, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
}

void st7796_displayInit(void)
{
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_BL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(LCD_PIN_BL, LCD_BK_LIGHT_OFF_LEVEL);

    ESP_LOGI(TAG, "Initialize SPI bus host=%d sck=%d mosi=%d miso=%d",
             LCD_HOST, LCD_PIN_SCK, LCD_PIN_MOSI, LCD_PIN_MISO);
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_SCK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = LCD_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO: DC=%d CS=%d pclk=%dHz",
             LCD_PIN_DC, LCD_PIN_CS, LCD_PIXEL_CLOCK_HZ);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = display_notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
    io_handle_global = io_handle;

    ESP_LOGI(TAG, "Install ST7789 panel driver (base for ST7796), RST=%d", LCD_PIN_RST);
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    /* 硬件复位 */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    /* 发送 ST7796 自定义初始化序列 */
    ESP_LOGI(TAG, "Send ST7796 init commands (320x480 RGB565)");
    st7796_send_init_cmds(io_handle);

    /* 方向适配：默认竖屏 320x480，不swap_xy，可根据实际镜像 */
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));

    /* 显示开 */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(LCD_PIN_BL, LCD_BK_LIGHT_ON_LEVEL);
}
