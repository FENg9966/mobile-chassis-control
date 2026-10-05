
#ifndef MAIN_ST7796_H_
#define MAIN_ST7796_H_

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_CMD_BITS            8
#define LCD_PARAM_BITS          8

extern esp_lcd_panel_handle_t panel_handle;
extern esp_lcd_panel_io_handle_t io_handle_global;

void st7796_displayInit(void);

#ifdef __cplusplus
}
#endif

#endif
