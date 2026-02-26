//
// Copyright (c) 2022 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
// which is available at https://www.apache.org/licenses/LICENSE-2.0.
//
// SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
//
// Contributors:
//   ZettaScale Zenoh Team, <zenoh@zettascale.tech>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zenoh-pico.h>

#if Z_FEATURE_PUBLICATION == 1
#define ESP_WIFI_SSID "Sam_wifi"
#define ESP_WIFI_PASS "aaaaaaaa"
#define ESP_MAXIMUM_RETRY 5
#define WIFI_CONNECTED_BIT BIT0

static bool s_is_wifi_connected = false;
static EventGroupHandle_t s_event_group_handler;
static int s_retry_count = 0;

#define CLIENT_OR_PEER 0  // 0: Client mode; 1: Peer mode
#if CLIENT_OR_PEER == 0
#define MODE "client"
#define LOCATOR "tcp/10.243.82.169:7447" // If empty, it will scout
#elif CLIENT_OR_PEER == 1
#define MODE "peer"
#define LOCATOR "tcp/10.55.93.169:7447"
#else
#error "Unknown Zenoh operation mode. Check CLIENT_OR_PEER value."
#endif

#define KEYEXPR "imu/data"
#define VALUE "[ESPIDF]{ESP32} Publication from Zenoh-Pico!"





#define BNO_UART        UART_NUM_1
#define BNO_TX_PIN      17
#define BNO_RX_PIN      16
#define BNO_BAUD        115200
#define BUF_SIZE        1024


void uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = BNO_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(BNO_UART, &uart_config);
    uart_set_pin(BNO_UART, BNO_TX_PIN, BNO_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(BNO_UART, BUF_SIZE, 0, 0, NULL, 0);
}



static bool uart_recv_byte(uint8_t *out, int timeout_ms) {
    return uart_read_bytes(BNO_UART, out, 1, pdMS_TO_TICKS(timeout_ms)) == 1;
}


void return_BNO085_data(int16_t *BNO085_data) {
    uint8_t header_1, header_2;
    uint8_t yaw_1, yaw_2;
    uint8_t pitch_1, pitch_2;
    uint8_t roll_1, roll_2;
    uint8_t x_accel_1, x_accel_2;
    uint8_t y_accel_1, y_accel_2;
    uint8_t z_accel_1, z_accel_2;

    while (1) {
        if (!uart_recv_byte(&header_1, 100)) continue;
        if (header_1 != 0xAA) continue;

        if (!uart_recv_byte(&header_2, 100)) continue;
        if (header_2 != 0xAA) continue;

        
        uint8_t dummy;
        uart_recv_byte(&dummy,   100);  // duplicate read (bug in original?)
        uart_recv_byte(&yaw_1,   100);
        uart_recv_byte(&yaw_2,   100);
        uart_recv_byte(&pitch_1, 100);
        uart_recv_byte(&pitch_2, 100);
        uart_recv_byte(&roll_1,  100);
        uart_recv_byte(&roll_2,  100);
        uart_recv_byte(&x_accel_1, 100);
        uart_recv_byte(&x_accel_2, 100);
        uart_recv_byte(&y_accel_1, 100);
        uart_recv_byte(&y_accel_2, 100);
        uart_recv_byte(&z_accel_1, 100);
        uart_recv_byte(&z_accel_2, 100);

        BNO085_data[0] = (int16_t)((yaw_2   << 8) | yaw_1);
        BNO085_data[1] = (int16_t)((pitch_2 << 8) | pitch_1);
        BNO085_data[2] = (int16_t)((roll_2  << 8) | roll_1);
        BNO085_data[3] = (int16_t)((x_accel_2 << 8) | x_accel_1);
        BNO085_data[4] = (int16_t)((y_accel_2 << 8) | y_accel_1);
        BNO085_data[5] = (int16_t)((z_accel_2 << 8) | z_accel_1);
        break;
    }
}



























static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_event_group_handler, WIFI_CONNECTED_BIT);
        s_retry_count = 0;
    }
}

void wifi_init_sta(void) {
    s_event_group_handler = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    printf("1\n");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    printf("2\n");
    ESP_ERROR_CHECK(esp_wifi_init(&config));

    esp_event_handler_instance_t handler_any_id;
    esp_event_handler_instance_t handler_got_ip;
    printf("3\n");
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &handler_any_id));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &handler_got_ip));
printf("4\n");
    wifi_config_t wifi_config = {.sta = {
                                     .ssid = ESP_WIFI_SSID,
                                     .password = ESP_WIFI_PASS,
                                 }};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
printf("5\n");
    EventBits_t bits = xEventGroupWaitBits(s_event_group_handler, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        s_is_wifi_connected = true;
    }
printf("6\n");
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, handler_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, handler_any_id));
    vEventGroupDelete(s_event_group_handler);
}

void app_main() {

        for (int i=0; i<200; i++){
        printf("%d\n", i);
        vTaskDelay(pdMS_TO_TICKS(50));
            }
            uart_init();
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Set WiFi in STA mode and trigger attachment
    printf("Connecting to WiFi...");
    wifi_init_sta();
    while (!s_is_wifi_connected) {
        printf(".");
        sleep(1);
    }
    printf("OK!\n");

    // Initialize Zenoh Session and other parameters
    z_owned_config_t config;
    z_config_default(&config);
    zp_config_insert(z_loan_mut(config), Z_CONFIG_MODE_KEY, MODE);
    if (strcmp(LOCATOR, "") != 0) {
        if (strcmp(MODE, "client") == 0) {
            zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_KEY, LOCATOR);
        } else {
            zp_config_insert(z_loan_mut(config), Z_CONFIG_LISTEN_KEY, LOCATOR);
        }
    }

    // Open Zenoh session
    printf("Opening Zenoh Session...");
    z_owned_session_t s;
    if (z_open(&s, z_move(config), NULL) < 0) {
        printf("Unable to open session!\n");
        exit(-1);
    }
    printf("OK\n");

    // Start the receive and the session lease loop for zenoh-pico
    zp_start_read_task(z_loan_mut(s), NULL);
    zp_start_lease_task(z_loan_mut(s), NULL);

    printf("Declaring publisher for '%s'...", KEYEXPR);
    z_owned_publisher_t pub;
    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, KEYEXPR);
    if (z_declare_publisher(z_loan(s), &pub, z_loan(ke), NULL) < 0) {
        printf("Unable to declare publisher for key expression!\n");
        exit(-1);
    }
    printf("OK\n");

    char buf[256];
    int16_t BN_data[6];
    for (int idx = 0; 1; ++idx) {
        sprintf(buf, "[%4d] %s", idx, VALUE);
        
        return_BNO085_data(BN_data);


            char usr_str[200];
            sprintf(usr_str, "%d, %d, %d, %d, %d, %d\n", BN_data[0], BN_data[1], BN_data[2], BN_data[3], BN_data[4], BN_data[5]);
            printf("Putting Data ('%s': '%s')...\n", KEYEXPR, usr_str);
            z_owned_bytes_t payload;
            z_bytes_copy_from_str(&payload, usr_str);
            z_publisher_put(z_loan(pub), z_move(payload), NULL);
        // Create payload
        // z_owned_bytes_t payload;
        // z_bytes_copy_from_str(&payload, buf);

        // z_publisher_put(z_loan(pub), z_move(payload), NULL);

    }
    printf("Closing Zenoh Session...");
    z_drop(z_move(pub));

    z_drop(z_move(s));
    printf("OK!\n");
}
#else
void app_main() {
    printf("ERROR: Zenoh pico was compiled without Z_FEATURE_PUBLICATION but this example requires it.\n");
}
#endif