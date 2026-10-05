#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bridge_config.h"
#include "chassis_protocol.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

typedef struct {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    chassis_packet_t packet;
} wireless_rx_item_t;

static const char *TAG = "chassis_bridge";
static QueueHandle_t s_wireless_rx_queue;
static uint8_t s_remote_mac[ESP_NOW_ETH_ALEN];
static bool s_remote_known = false;
static int64_t s_last_command_us = 0;
static uint16_t s_stop_sequence = 0;

static void ensure_remote_peer(const uint8_t *mac)
{
    if (esp_now_is_peer_exist(mac)) {
        return;
    }

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
    peer.channel = BRIDGE_ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGW(TAG, "Add handheld peer failed: %s", esp_err_to_name(err));
    }
}

static void wireless_receive_cb(const esp_now_recv_info_t *info,
                                const uint8_t *data, int data_len)
{
    if (info == NULL || data_len != sizeof(chassis_packet_t)) {
        return;
    }

    /* Pair with the first valid handheld after boot and ignore other remotes. */
    if (s_remote_known &&
        memcmp(info->src_addr, s_remote_mac, ESP_NOW_ETH_ALEN) != 0) {
        return;
    }

    wireless_rx_item_t item;
    memcpy(item.source_mac, info->src_addr, ESP_NOW_ETH_ALEN);
    memcpy(&item.packet, data, sizeof(item.packet));
    if (!chassis_packet_is_valid(&item.packet, sizeof(item.packet)) ||
        item.packet.type != CHASSIS_MSG_COMMAND ||
        item.packet.command > CHASSIS_CMD_RIGHT ||
        item.packet.speed_percent > 100U) {
        return;
    }
    (void)xQueueSend(s_wireless_rx_queue, &item, 0);
}

static void send_ack(const uint8_t *destination, uint16_t sequence)
{
    chassis_packet_t ack = {0};
    ack.type = CHASSIS_MSG_ACK;
    ack.sequence = sequence;
    ack.command = CHASSIS_CMD_STOP;
    ack.status = CHASSIS_STATUS_LINK_OK;
    chassis_packet_finalize(&ack);
    (void)esp_now_send(destination, (const uint8_t *)&ack, sizeof(ack));
}

static void uart_send_packet(chassis_packet_t *packet)
{
    chassis_packet_finalize(packet);
    uart_write_bytes(BRIDGE_UART_PORT, packet, sizeof(*packet));
}

static void send_failsafe_stop(void)
{
    chassis_packet_t stop = {0};
    stop.type = CHASSIS_MSG_COMMAND;
    stop.sequence = ++s_stop_sequence;
    stop.command = CHASSIS_CMD_STOP;
    stop.speed_percent = 0;
    uart_send_packet(&stop);
}

static void bridge_task(void *argument)
{
    (void)argument;
    wireless_rx_item_t item;
    int64_t last_stop_us = 0;

    send_failsafe_stop();
    while (true) {
        if (xQueueReceive(s_wireless_rx_queue, &item, pdMS_TO_TICKS(20)) == pdTRUE) {
            memcpy(s_remote_mac, item.source_mac, ESP_NOW_ETH_ALEN);
            s_remote_known = true;
            ensure_remote_peer(s_remote_mac);

            s_last_command_us = esp_timer_get_time();
            uart_write_bytes(BRIDGE_UART_PORT, &item.packet, sizeof(item.packet));
            send_ack(s_remote_mac, item.packet.sequence);
        }

        const int64_t now = esp_timer_get_time();
        if ((s_last_command_us == 0 ||
             now - s_last_command_us >
                 (int64_t)CHASSIS_WIRELESS_TIMEOUT_MS * 1000) &&
            now - last_stop_us > 100000) {
            send_failsafe_stop();
            last_stop_us = now;
        }
    }
}

static void process_uart_byte(uint8_t byte)
{
    static uint8_t frame[sizeof(chassis_packet_t)];
    static size_t used = 0;

    if (used == 0 && byte != (uint8_t)(CHASSIS_PACKET_MAGIC & 0xFFU)) {
        return;
    }
    if (used == 1 && byte != (uint8_t)(CHASSIS_PACKET_MAGIC >> 8)) {
        used = (byte == (uint8_t)(CHASSIS_PACKET_MAGIC & 0xFFU)) ? 1U : 0U;
        frame[0] = byte;
        return;
    }

    frame[used++] = byte;
    if (used < sizeof(frame)) {
        return;
    }

    chassis_packet_t packet;
    memcpy(&packet, frame, sizeof(packet));
    used = 0;
    if (!chassis_packet_is_valid(&packet, sizeof(packet)) ||
        packet.type != CHASSIS_MSG_TELEMETRY || !s_remote_known) {
        return;
    }

    ensure_remote_peer(s_remote_mac);
    (void)esp_now_send(s_remote_mac, (const uint8_t *)&packet, sizeof(packet));
}

static void uart_receive_task(void *argument)
{
    (void)argument;
    uint8_t buffer[64];
    while (true) {
        int received = uart_read_bytes(BRIDGE_UART_PORT, buffer, sizeof(buffer),
                                       pdMS_TO_TICKS(20));
        for (int i = 0; i < received; ++i) {
            process_uart_byte(buffer[i]);
        }
    }
}

static esp_err_t init_uart(void)
{
    const uart_config_t config = {
        .baud_rate = BRIDGE_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(BRIDGE_UART_PORT, 1024, 1024,
                                             0, NULL, 0), TAG, "UART driver");
    ESP_RETURN_ON_ERROR(uart_param_config(BRIDGE_UART_PORT, &config),
                        TAG, "UART config");
    return uart_set_pin(BRIDGE_UART_PORT, BRIDGE_UART_TX_PIN,
                        BRIDGE_UART_RX_PIN,
                        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static esp_err_t init_espnow(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "NVS init");
    ESP_ERROR_CHECK(esp_netif_init());
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_config), TAG, "Wi-Fi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Wi-Fi storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Wi-Fi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start");
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "Wi-Fi power save");
    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(BRIDGE_ESPNOW_CHANNEL,
                                              WIFI_SECOND_CHAN_NONE),
                        TAG, "Wi-Fi channel");
    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "ESP-NOW init");
    return esp_now_register_recv_cb(wireless_receive_cb);
}

void app_main(void)
{
    s_wireless_rx_queue = xQueueCreate(16, sizeof(wireless_rx_item_t));
    ESP_ERROR_CHECK(s_wireless_rx_queue == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    ESP_ERROR_CHECK(init_uart());
    ESP_ERROR_CHECK(init_espnow());

    uint8_t local_mac[6];
    ESP_ERROR_CHECK(esp_read_mac(local_mac, ESP_MAC_WIFI_STA));
    ESP_LOGI(TAG, "Bridge MAC %02X:%02X:%02X:%02X:%02X:%02X channel %d",
             local_mac[0], local_mac[1], local_mac[2],
             local_mac[3], local_mac[4], local_mac[5],
             BRIDGE_ESPNOW_CHANNEL);
    ESP_LOGI(TAG, "UART1 TX=GPIO%d RX=GPIO%d baud=%d",
             BRIDGE_UART_TX_PIN, BRIDGE_UART_RX_PIN, BRIDGE_UART_BAUD);

    xTaskCreate(bridge_task, "bridge", 4096, NULL, 6, NULL);
    xTaskCreate(uart_receive_task, "uart_rx", 4096, NULL, 5, NULL);
}

