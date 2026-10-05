
#ifndef MAIN_TOUCH_H_
#define MAIN_TOUCH_H_

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "esp_err.h"
#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool pressed;
    uint16_t x;
    uint16_t y;
} touch_point_t;

esp_err_t touch_init(void);
bool touch_read(touch_point_t *tp);
void touch_lvgl_init(lv_disp_t *disp);

#ifdef __cplusplus
}
#endif

#endif
