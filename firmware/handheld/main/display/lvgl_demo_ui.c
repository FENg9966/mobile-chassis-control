#include "lvgl_demo_ui.h"

#include <stdint.h>
#include <stdio.h>
#include "esp_log.h"
#include "wireless/espnow_remote.h"

static const char *TAG = "lvgl_ui";
static lv_obj_t *s_command_label;
static lv_obj_t *s_link_label;
static lv_obj_t *s_speed_label;
static lv_obj_t *s_slider;
static chassis_command_t s_active_command = CHASSIS_CMD_STOP;

static const char *command_name(chassis_command_t command)
{
    switch (command) {
    case CHASSIS_CMD_FORWARD: return "FWD";
    case CHASSIS_CMD_REVERSE: return "REV";
    case CHASSIS_CMD_LEFT: return "LEFT";
    case CHASSIS_CMD_RIGHT: return "RIGHT";
    case CHASSIS_CMD_STOP:
    default: return "STOP";
    }
}

static uint8_t current_speed(void)
{
    return (uint8_t)lv_slider_get_value(s_slider);
}

static void show_command(chassis_command_t command)
{
    s_active_command = command;
    lv_label_set_text_fmt(s_command_label, "Command: %s", command_name(command));
}

static void drive_button_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    const chassis_command_t command =
        (chassis_command_t)(intptr_t)lv_event_get_user_data(event);

    if (code == LV_EVENT_PRESSED) {
        espnow_remote_set_command(command, current_speed());
        show_command(command);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        espnow_remote_force_stop();
        show_command(CHASSIS_CMD_STOP);
    }
}

static void stop_button_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) {
        espnow_remote_force_stop();
        show_command(CHASSIS_CMD_STOP);
    }
}

static void speed_slider_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    const uint8_t speed = current_speed();
    lv_label_set_text_fmt(s_speed_label, "%u%%", speed);
    if (s_active_command != CHASSIS_CMD_STOP) {
        espnow_remote_set_command(s_active_command, speed);
    }
}

static void status_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    chassis_packet_t telemetry;

    if (!espnow_remote_is_linked()) {
        lv_label_set_text(s_link_label, "Wireless: WAIT / SAFE STOP");
        lv_obj_set_style_text_color(s_link_label, lv_color_hex(0xFF6B6B), 0);
        return;
    }

    if (espnow_remote_get_telemetry(&telemetry)) {
        lv_label_set_text_fmt(s_link_label, "LINK OK %u.%02uV L:%d R:%d",
                              telemetry.battery_mv / 1000U,
                              (telemetry.battery_mv % 1000U) / 10U,
                              telemetry.left_rpm, telemetry.right_rpm);
    } else {
        lv_label_set_text(s_link_label, "Wireless: LINK OK");
    }
    lv_obj_set_style_text_color(s_link_label, lv_color_hex(0x43E97B), 0);
}

static lv_obj_t *make_drive_button(lv_obj_t *parent, const char *text,
                                   chassis_command_t command,
                                   lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 82, 66);
    lv_obj_set_style_radius(button, 14, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x19D3E6), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x118FA0), LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, callback, LV_EVENT_ALL,
                        (void *)(intptr_t)command);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_center(label);
    return button;
}

void example_lvgl_demo_ui(lv_disp_t *disp)
{
    ESP_LOGI(TAG, "Create handheld chassis remote UI");
    lv_obj_t *screen = lv_disp_get_scr_act(disp);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101827), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "MOBILE CHASSIS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x20A4F3), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t *subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "ESP32-S3 | ESP-NOW | H743");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xB9C7D5), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 45);

    lv_obj_t *speed_panel = lv_obj_create(screen);
    lv_obj_set_size(speed_panel, 292, 96);
    lv_obj_align(speed_panel, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_bg_color(speed_panel, lv_color_hex(0x203047), 0);
    lv_obj_set_style_border_width(speed_panel, 0, 0);
    lv_obj_set_style_radius(speed_panel, 18, 0);
    lv_obj_clear_flag(speed_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_speed_label = lv_label_create(speed_panel);
    lv_label_set_text(s_speed_label, "40%");
    lv_obj_set_style_text_color(s_speed_label, lv_color_hex(0x43E97B), 0);
    lv_obj_set_style_text_font(s_speed_label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_speed_label, LV_ALIGN_TOP_RIGHT, -8, -2);

    s_slider = lv_slider_create(speed_panel);
    lv_obj_set_size(s_slider, 250, 18);
    lv_obj_align(s_slider, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_slider_set_range(s_slider, 0, 100);
    lv_slider_set_value(s_slider, 40, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_slider, speed_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *drive_panel = lv_obj_create(screen);
    lv_obj_set_size(drive_panel, 292, 226);
    lv_obj_align(drive_panel, LV_ALIGN_TOP_MID, 0, 178);
    lv_obj_set_style_bg_color(drive_panel, lv_color_hex(0x203047), 0);
    lv_obj_set_style_border_width(drive_panel, 0, 0);
    lv_obj_set_style_radius(drive_panel, 18, 0);
    lv_obj_clear_flag(drive_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_command_label = lv_label_create(drive_panel);
    lv_label_set_text(s_command_label, "Command: STOP");
    lv_obj_set_style_text_color(s_command_label, lv_color_hex(0xFFD166), 0);
    lv_obj_align(s_command_label, LV_ALIGN_TOP_MID, 0, -2);

    lv_obj_t *forward = make_drive_button(drive_panel, "FWD",
                                          CHASSIS_CMD_FORWARD, drive_button_cb);
    lv_obj_align(forward, LV_ALIGN_TOP_LEFT, -2, 34);
    lv_obj_t *stop = make_drive_button(drive_panel, "STOP",
                                       CHASSIS_CMD_STOP, stop_button_cb);
    lv_obj_align(stop, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_t *reverse = make_drive_button(drive_panel, "REV",
                                          CHASSIS_CMD_REVERSE, drive_button_cb);
    lv_obj_align(reverse, LV_ALIGN_TOP_RIGHT, 2, 34);
    lv_obj_t *left = make_drive_button(drive_panel, "LEFT",
                                       CHASSIS_CMD_LEFT, drive_button_cb);
    lv_obj_align(left, LV_ALIGN_BOTTOM_LEFT, 40, 0);
    lv_obj_t *right = make_drive_button(drive_panel, "RIGHT",
                                        CHASSIS_CMD_RIGHT, drive_button_cb);
    lv_obj_align(right, LV_ALIGN_BOTTOM_RIGHT, -40, 0);

    s_link_label = lv_label_create(screen);
    lv_label_set_text(s_link_label, "Wireless: WAIT / SAFE STOP");
    lv_obj_set_style_text_color(s_link_label, lv_color_hex(0xFF6B6B), 0);
    lv_obj_align(s_link_label, LV_ALIGN_BOTTOM_MID, 0, -18);

    lv_timer_create(status_timer_cb, 200, NULL);
    espnow_remote_force_stop();
}
