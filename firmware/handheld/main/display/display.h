
#ifndef MAIN_DISPLAY_H_
#define MAIN_DISPLAY_H_

#include "st7796.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "board.h"

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2

extern lv_disp_drv_t disp_drv;

bool display_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);

void displayConfig(void);

#endif
