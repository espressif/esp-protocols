/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "eppp_link.h"
#include "inttypes.h"
#include "esp_http_server.h"
#include "cJSON.h"

static const char *TAG = "eppp_slave";

#if defined(CONFIG_SOC_WIFI_SUPPORTED) && !defined(CONFIG_EXAMPLE_WIFI_OVER_EPPP_CHANNEL)

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_DISCONNECT_DONE_BIT  BIT2

static volatile bool s_manual_reconnect = false;

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        wifi_config_t cfg;
        if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK && cfg.sta.ssid[0] != 0) {
            esp_wifi_connect();
        } else {
            ESP_LOGI(TAG, "STA_START: no stored SSID, waiting for config");
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "DISCONNECTED, reason=%d", disc ? disc->reason : -1);

        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECT_DONE_BIT);

        if (!s_manual_reconnect) {
            if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
                esp_err_t err = esp_wifi_connect();
                if (err == ESP_ERR_WIFI_CONN) {
                    ESP_LOGW(TAG, "esp_wifi_connect(): already connecting");
                } else if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_wifi_connect() failed: 0x%x", (unsigned)err);
                } else {
                    s_retry_num++;
                    ESP_LOGI(TAG, "retry to connect to the AP (%d)", s_retry_num);
                }
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
        } else {
            s_retry_num = 0;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&ev->ip_info.ip));
        s_retry_num = 0;
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "CONNECTED to AP");
    } else {
        ESP_LOGI(TAG, "Unhandled event: base=%s id=%" PRIi32, event_base, event_id);
    }
}



void init_network_interface(void)
{
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    if (wifi_config.sta.ssid[0] == 0) {
        ESP_LOGW(TAG, "No default SSID configured, skipping wait; configure via REST");
        return;
    }
    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}
#else

// If the SoC does not have WiFi capabilities, we can initialize a different network interface, this function is a placeholder for that purpose.
// This function is also a no-op if EXAMPLE_WIFI_OVER_EPPP_CHANNEL==1, since the Wi-Fi network interface will live on the other peer (on the host side).
void init_network_interface(void)
{
}

#endif // SoC WiFi capable chip || WiFi over EPPP channel

static const char *TAG_REST = "wifi_rest";

/* ---------- WiFi REST API --------- */
esp_err_t wifi_status_handler(httpd_req_t *req)
{
    wifi_ap_record_t ap_info;
    char buf[128];

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        snprintf(buf, sizeof(buf),
                 "{\"status\":\"connected\",\"ssid\":\"%s\",\"rssi\":%d}",
                 (char *)ap_info.ssid, ap_info.rssi);
    } else {
        snprintf(buf, sizeof(buf), "{\"status\":\"disconnected\"}");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_conf = {0};
    wifi_ap_record_t ap_records[16];
    uint16_t ap_num = 16;

    esp_wifi_scan_start(&scan_conf, true);
    esp_wifi_scan_get_ap_records(&ap_num, ap_records);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ap_num; ++i) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (char*)ap_records[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", ap_records[i].rssi);
        cJSON_AddNumberToObject(ap, "auth", ap_records[i].authmode);
        cJSON_AddItemToArray(root, ap);
    }
    char *resp = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, resp);

    cJSON_Delete(root);
    free(resp);
    return ESP_OK;
}

esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    char buf[256] = {0};
    httpd_req_recv(req, buf, sizeof(buf)-1);

    cJSON *json = cJSON_Parse(buf);
    if (!json) { /* ... 400 ... */ return ESP_FAIL; }

    cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
    cJSON *pass = cJSON_GetObjectItem(json, "password");
    if (!ssid || !ssid->valuestring) { /* ... 400 ... */ cJSON_Delete(json); return ESP_FAIL; }

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid->valuestring, sizeof(wifi_config.sta.ssid)-1);
    if (pass && pass->valuestring) {
        strncpy((char *)wifi_config.sta.password, pass->valuestring, sizeof(wifi_config.sta.password)-1);
    }
    cJSON_Delete(json);

    s_manual_reconnect = true;

    xEventGroupClearBits(s_wifi_event_group, WIFI_DISCONNECT_DONE_BIT);
    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT) {
        s_manual_reconnect = false;
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "disconnect failed");
    }

    (void)xEventGroupWaitBits(s_wifi_event_group, WIFI_DISCONNECT_DONE_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));

    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_CONN) {
        ESP_LOGW(TAG, "Already connecting; continue");
        err = ESP_OK;
    }
    s_manual_reconnect = false;

    if (err != ESP_OK) {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "connect failed");
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{\"result\":\"ok\"}");
    return ESP_OK;
}


esp_err_t wifi_disconnect_handler(httpd_req_t *req)
{
    s_manual_reconnect = true;
    xEventGroupClearBits(s_wifi_event_group, WIFI_DISCONNECT_DONE_BIT);

    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT) {
        s_manual_reconnect = false;
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "disconnect failed");
    }

    (void)xEventGroupWaitBits(s_wifi_event_group, WIFI_DISCONNECT_DONE_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));

    s_manual_reconnect = false;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{\"result\":\"ok\"}");
    return ESP_OK;
}


static esp_err_t cors_preflight_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin",  "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_sendstr(req, "");
    return ESP_OK;
}

void start_rest_server()
{
     httpd_config_t config = HTTPD_DEFAULT_CONFIG();
     config.uri_match_fn = httpd_uri_match_wildcard;
     httpd_handle_t server = NULL;
     httpd_start(&server, &config);

     // CORS pre-flight handler for OPTIONS
     // Register OPTIONS handler on all URIs
     httpd_uri_t uri_options = {
         .uri      = "/*",
         .method   = HTTP_OPTIONS,
         .handler  = cors_preflight_handler
     };
     httpd_register_uri_handler(server, &uri_options);


    // /wifi/status
    httpd_uri_t uri_status = {
        .uri       = "/wifi/status",
        .method    = HTTP_GET,
        .handler   = wifi_status_handler
    };
    httpd_register_uri_handler(server, &uri_status);

    // /wifi/scan
    httpd_uri_t uri_scan = {
        .uri       = "/wifi/scan",
        .method    = HTTP_GET,
        .handler   = wifi_scan_handler
    };
    httpd_register_uri_handler(server, &uri_scan);

    // /wifi/connect
    httpd_uri_t uri_connect = {
        .uri       = "/wifi/connect",
        .method    = HTTP_POST,
        .handler   = wifi_connect_handler
    };
    httpd_register_uri_handler(server, &uri_connect);

    // /wifi/disconnect
    httpd_uri_t uri_disconnect = {
        .uri       = "/wifi/disconnect",
        .method    = HTTP_POST,
        .handler   = wifi_disconnect_handler
    };
    httpd_register_uri_handler(server, &uri_disconnect);

    ESP_LOGI(TAG_REST, "REST Wi-Fi API started on PPPoS IP");
}


void station_over_eppp_channel(void *arg);

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

     ESP_ERROR_CHECK(esp_netif_init());
     ESP_ERROR_CHECK(esp_event_loop_create_default());

     init_network_interface();

     start_rest_server();
    eppp_config_t config = EPPP_DEFAULT_SERVER_CONFIG();
#if CONFIG_EPPP_LINK_DEVICE_SPI
    config.transport = EPPP_TRANSPORT_SPI;
    config.spi.is_master = false;
    config.spi.host = CONFIG_EXAMPLE_SPI_HOST;
    config.spi.mosi = CONFIG_EXAMPLE_SPI_MOSI_PIN;
    config.spi.miso = CONFIG_EXAMPLE_SPI_MISO_PIN;
    config.spi.sclk = CONFIG_EXAMPLE_SPI_SCLK_PIN;
    config.spi.cs = CONFIG_EXAMPLE_SPI_CS_PIN;
    config.spi.intr = CONFIG_EXAMPLE_SPI_INTR_PIN;
    config.spi.freq = CONFIG_EXAMPLE_SPI_FREQUENCY;
#elif CONFIG_EPPP_LINK_DEVICE_UART
    config.transport = EPPP_TRANSPORT_UART;
    config.uart.tx_io = CONFIG_EXAMPLE_UART_TX_PIN;
    config.uart.rx_io = CONFIG_EXAMPLE_UART_RX_PIN;
    config.uart.baud = CONFIG_EXAMPLE_UART_BAUDRATE;
#elif CONFIG_EPPP_LINK_DEVICE_SDIO
    config.transport = EPPP_TRANSPORT_SDIO;
#endif

    esp_netif_t *eppp_netif = eppp_listen(&config);
    if (eppp_netif == NULL) {
        ESP_LOGE(TAG, "Failed to setup connection");
        return ;
    }
#ifdef CONFIG_EXAMPLE_WIFI_OVER_EPPP_CHANNEL
    station_over_eppp_channel(eppp_netif);
#else
    {
         esp_err_t err = esp_netif_napt_enable(eppp_netif);
         if (err == ESP_OK) {
             ESP_LOGI(TAG, "NAPT enabled on PPP interface");
         } else {
             ESP_LOGE(TAG, "NAPT enable failed: %d", err);
         }
     }
#endif // CONFIG_EXAMPLE_WIFI_OVER_EPPP_CHANNEL
}