#include "espnow_remote.h"

#include <string.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define ESPNOW_CHANNEL 6

static const char *TAG = "remote";
static const uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static chassis_command_t s_command = CHASSIS_CMD_STOP;
static uint8_t s_speed_percent = 0;
static uint16_t s_sequence = 0;
static int64_t s_last_ack_us = 0;
static chassis_packet_t s_telemetry;
static bool s_has_telemetry = false;

static void remote_receive_cb(const esp_now_recv_info_t *info,
                              const uint8_t *data, int data_len)
{
    (void)info;
    if (data_len != sizeof(chassis_packet_t)) {
        return;
    }

    chassis_packet_t packet;
    memcpy(&packet, data, sizeof(packet));
    if (!chassis_packet_is_valid(&packet, sizeof(packet))) {
        return;
    }

    if (packet.type == CHASSIS_MSG_ACK) {
        portENTER_CRITICAL(&s_lock);
        s_last_ack_us = esp_timer_get_time();
        portEXIT_CRITICAL(&s_lock);
    } else if (packet.type == CHASSIS_MSG_TELEMETRY) {
        portENTER_CRITICAL(&s_lock);
        s_telemetry = packet;
        s_has_telemetry = true;
        s_last_ack_us = esp_timer_get_time();
        portEXIT_CRITICAL(&s_lock);
    }
}

static void remote_send_task(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        chassis_packet_t packet = {0};

        portENTER_CRITICAL(&s_lock);
        packet.sequence = ++s_sequence;
        packet.command = (uint8_t)s_command;
        packet.speed_percent = s_speed_percent;
        portEXIT_CRITICAL(&s_lock);

        packet.type = CHASSIS_MSG_COMMAND;
        chassis_packet_finalize(&packet);

        esp_err_t err = esp_now_send(s_broadcast_mac,
                                     (const uint8_t *)&packet,
                                     sizeof(packet));
        if (err != ESP_OK && err != ESP_ERR_ESPNOW_NO_MEM) {
            ESP_LOGW(TAG, "ESP-NOW send failed: %s", esp_err_to_name(err));
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CHASSIS_COMMAND_PERIOD_MS));
    }
}

esp_err_t espnow_remote_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "NVS init failed");

    ESP_ERROR_CHECK(esp_netif_init());
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_config), TAG, "Wi-Fi init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Wi-Fi storage failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Wi-Fi mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "Wi-Fi power save failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE),
                        TAG, "Wi-Fi channel failed");

    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "ESP-NOW init failed");
    ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(remote_receive_cb),
                        TAG, "ESP-NOW receive callback failed");

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_RETURN_ON_ERROR(esp_now_add_peer(&peer), TAG, "Broadcast peer failed");

    uint8_t local_mac[6];
    ESP_ERROR_CHECK(esp_read_mac(local_mac, ESP_MAC_WIFI_STA));
    ESP_LOGI(TAG, "Handheld MAC %02X:%02X:%02X:%02X:%02X:%02X, channel %d",
             local_mac[0], local_mac[1], local_mac[2],
             local_mac[3], local_mac[4], local_mac[5], ESPNOW_CHANNEL);

    if (xTaskCreate(remote_send_task, "remote_tx", 4096, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void espnow_remote_set_command(chassis_command_t command, uint8_t speed_percent)
{
    if (speed_percent > 100U) {
        speed_percent = 100U;
    }
    if (command == CHASSIS_CMD_STOP) {
        speed_percent = 0U;
    }

    portENTER_CRITICAL(&s_lock);
    s_command = command;
    s_speed_percent = speed_percent;
    portEXIT_CRITICAL(&s_lock);
}

void espnow_remote_force_stop(void)
{
    espnow_remote_set_command(CHASSIS_CMD_STOP, 0);
}

bool espnow_remote_is_linked(void)
{
    int64_t last_ack;
    portENTER_CRITICAL(&s_lock);
    last_ack = s_last_ack_us;
    portEXIT_CRITICAL(&s_lock);

    return last_ack != 0 &&
           (esp_timer_get_time() - last_ack) <
               ((int64_t)CHASSIS_WIRELESS_TIMEOUT_MS * 1000);
}

bool espnow_remote_get_telemetry(chassis_packet_t *telemetry)
{
    if (telemetry == NULL) {
        return false;
    }

    bool available;
    portENTER_CRITICAL(&s_lock);
    available = s_has_telemetry;
    if (available) {
        *telemetry = s_telemetry;
    }
    portEXIT_CRITICAL(&s_lock);
    return available;
}
