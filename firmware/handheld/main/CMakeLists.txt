
idf_component_register(
    SRCS "main.c"

    SRCS "display/st7796.c"
    SRCS "display/display.c"
    SRCS "display/lvgl_demo_ui.c"
    SRCS "display/lv_font_montserrat_20.c"

    SRCS "hardware/touch.c"

    SRCS "wireless/chassis_protocol.c"
    SRCS "wireless/espnow_remote.c"

    INCLUDE_DIRS "."
                  "display"
                  "hardware"
                  "wireless"

    REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash esp_lcd driver)
