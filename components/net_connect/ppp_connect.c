/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * PPP driver, over USB CDC or UART.
 *
 * start() prepares the transport and the netif, then returns.
 * connect() begins PPP negotiation and returns. It does not wait for an
 * address. net_iface.c marks the interface ready when the IP event arrives.
 * A lost-IP event retries until max_retry is used up.
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "net_connect.h"

#if CONFIG_NET_CONNECT_PPP

#include "net_iface_priv.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_NET_CONNECT_PPP_DEVICE_USB
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_default_config.h"

/* USB CDC port used for PPP. Updated when the line state changes. */
static int s_itf = TINYUSB_CDC_ACM_0;
static uint8_t s_rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE];
static bool s_usb_installed = false;
#else
#include "driver/uart.h"
#define PPP_UART_BUF_SIZE (1024)
/* Set to ask the UART task to leave its loop. */
static bool s_stop_task = false;
static TaskHandle_t s_ppp_task = NULL;
#endif

static const char *TAG = "net_connect_ppp";
static esp_netif_t *s_netif = NULL;
static bool s_lost_ip_handler_registered = false;

/* Send one PPP frame out the USB or UART port. */
static esp_err_t transmit(void *h, void *buffer, size_t len)
{
    (void)h;
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, len, ESP_LOG_VERBOSE);
#if CONFIG_NET_CONNECT_PPP_DEVICE_USB
    tinyusb_cdcacm_write_queue(s_itf, buffer, len);
    tinyusb_cdcacm_write_flush(s_itf, 0);
#else
    uart_write_bytes(UART_NUM_1, buffer, len);
#endif
    return ESP_OK;
}

/*
 * Single PPP driver object.
 * The handle must be non-NULL so esp_netif accepts the driver.
 */
static esp_netif_driver_ifconfig_t s_ppp_driver_cfg = {
    .handle = (void *)1,
    .transmit = transmit,
};

#if CONFIG_NET_CONNECT_PPP_DEVICE_USB
/* Pass bytes received on the USB CDC port into the PPP netif. */
static void cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    (void)event;
    if (itf != s_itf || s_netif == NULL) {
        return;
    }
    size_t rx_size = 0;
    esp_err_t ret = tinyusb_cdcacm_read(itf, s_rx_buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, s_rx_buf, rx_size, ESP_LOG_VERBOSE);
        esp_netif_receive(s_netif, s_rx_buf, rx_size, NULL);
    } else {
        ESP_LOGE(TAG, "USB read error");
    }
}

/* Remember which CDC port the host opened. */
static void line_state_changed(int itf, cdcacm_event_t *event)
{
    (void)event;
    s_itf = itf;
    ESP_LOGI(TAG, "Line state changed on channel %d", itf);
}

/* Install TinyUSB and the CDC callback. */
static esp_err_t ppp_usb_install(void)
{
    if (s_usb_installed) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "USB initialization");
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = &cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL,
    };
    err = tinyusb_cdcacm_init(&acm_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        tinyusb_driver_uninstall();
        return err;
    }
    err = tinyusb_cdcacm_register_callback(TINYUSB_CDC_ACM_0,
                                           CDC_EVENT_LINE_STATE_CHANGED,
                                           &line_state_changed);
    if (err != ESP_OK) {
        tinyusb_driver_uninstall();
        return err;
    }
    s_usb_installed = true;
    return ESP_OK;
}

/* Remove the CDC callback and the TinyUSB driver. */
static void ppp_usb_uninstall(void)
{
    if (!s_usb_installed) {
        return;
    }
    tinyusb_cdcacm_register_callback(TINYUSB_CDC_ACM_0, CDC_EVENT_LINE_STATE_CHANGED, NULL);
    tinyusb_driver_uninstall();
    s_itf = TINYUSB_CDC_ACM_0;
    s_usb_installed = false;
}
#else
/*
 * UART PPP worker.
 * Starts the netif, then copies UART bytes into it until stop is requested.
 */
static void ppp_task(void *arg)
{
    net_iface_handle_t self = (net_iface_handle_t)arg;

    uart_config_t uart_config = {
        .baud_rate = CONFIG_NET_CONNECT_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    QueueHandle_t event_queue;
    esp_err_t err = uart_driver_install(UART_NUM_1, PPP_UART_BUF_SIZE, 0, 16, &event_queue, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        net_iface_notify_failed(self, err);
        s_ppp_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    err = uart_param_config(UART_NUM_1, &uart_config);
    if (err == ESP_OK) {
        err = uart_set_pin(UART_NUM_1, CONFIG_NET_CONNECT_UART_TX_PIN,
                           CONFIG_NET_CONNECT_UART_RX_PIN,
                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (err == ESP_OK) {
        err = uart_set_rx_timeout(UART_NUM_1, 1);
    }
    if (err != ESP_OK) {
        /* A bad pin or config must fail the interface, not abort the chip. */
        ESP_LOGE(TAG, "UART setup failed: %s", esp_err_to_name(err));
        uart_driver_delete(UART_NUM_1);
        net_iface_notify_failed(self, err);
        s_ppp_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    char *buffer = malloc(PPP_UART_BUF_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for UART buffer");
        uart_driver_delete(UART_NUM_1);
        net_iface_notify_failed(self, ESP_ERR_NO_MEM);
        s_ppp_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected, s_netif);
    esp_netif_action_start(s_netif, 0, 0, 0);
    esp_netif_action_connected(s_netif, 0, 0, 0);

    uart_event_t event;
    while (!s_stop_task) {
        if (xQueueReceive(event_queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (event.type == UART_DATA) {
                /* A timeout means the PPP frame is complete. Read the whole buffer then. */
                if (event.timeout_flag) {
                    size_t len = 0;
                    uart_get_buffered_data_len(UART_NUM_1, &len);
                    if (len) {
                        len = uart_read_bytes(UART_NUM_1, buffer, PPP_UART_BUF_SIZE, 0);
                        ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, len, ESP_LOG_VERBOSE);
                        esp_netif_receive(s_netif, buffer, len, NULL);
                    }
                }
            } else {
                ESP_LOGW(TAG, "Received UART event: %d", event.type);
            }
        }
    }

    esp_event_handler_unregister(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected);
    uart_driver_delete(UART_NUM_1);
    free(buffer);
    s_ppp_task = NULL;
    vTaskDelete(NULL);
}

/* Ask the UART task to exit, then wait until it has left. */
static void ppp_stop_uart_task(void)
{
    if (s_ppp_task == NULL) {
        return;
    }
    s_stop_task = true;
    vTaskDelay(pdMS_TO_TICKS(1000));
    s_ppp_task = NULL;
}
#endif /* CONFIG_NET_CONNECT_PPP_DEVICE_USB */

/*
 * PPP lost its address.
 * Try again until the configured retry count is used up, then mark failed.
 */
static void on_ppp_lost_ip(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    (void)event_base;
    (void)event_id;
    net_iface_handle_t self = (net_iface_handle_t)arg;
    if (self == NULL || s_netif == NULL || event_data == NULL) {
        return;
    }

    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (event->esp_netif != s_netif) {
        return;
    }

    /* Stop and disconnect already own the netif. Do not start another attempt. */
    net_iface_state_t state = net_iface_get_state(self);
    if (state == NET_IFACE_STATE_STOPPING || state == NET_IFACE_STATE_INIT ||
            state == NET_IFACE_STATE_STARTED) {
        return;
    }

    xSemaphoreTake(self->lock, portMAX_DELAY);
    self->retry_num++;
    int retry_num = self->retry_num;
    int max_retry = self->cfg.ppp.max_retry;
    xSemaphoreGive(self->lock);

    if (retry_num > max_retry) {
        ESP_LOGE(TAG, "PPP connection failed %d times, stop reconnecting.", retry_num);
        net_iface_notify_failed(self, ESP_FAIL);
        return;
    }

    ESP_LOGI(TAG, "PPP connection failed %d times, try to reconnect.", retry_num);
    esp_netif_action_start(s_netif, 0, 0, 0);
    esp_netif_action_connected(s_netif, 0, 0, 0);
}

/* Create the PPP netif and listen for a lost address. The transport is prepared first. */
static esp_err_t ppp_ops_start(net_iface_handle_t self)
{
    if (s_netif != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

#if CONFIG_NET_CONNECT_PPP_DEVICE_USB
    esp_err_t err = ppp_usb_install();
    if (err != ESP_OK) {
        return err;
    }
#endif

    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_PPP();
    base.if_desc = self->cfg.desc != NULL ? self->cfg.desc : NET_CONNECT_NETIF_DESC_PPP;
    base.route_prio = self->cfg.route_prio > 0 ? self->cfg.route_prio : NET_CONNECT_PPP_DEFAULT_ROUTE_PRIO;
    esp_netif_config_t netif_cfg = {
        .base = &base,
        .driver = &s_ppp_driver_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_PPP,
    };

    s_netif = esp_netif_new(&netif_cfg);
    if (s_netif == NULL) {
#if CONFIG_NET_CONNECT_PPP_DEVICE_USB
        ppp_usb_uninstall();
#endif
        return ESP_ERR_NO_MEM;
    }

    esp_err_t reg_err = esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_LOST_IP,
                                                   on_ppp_lost_ip, self);
    if (reg_err != ESP_OK) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
#if CONFIG_NET_CONNECT_PPP_DEVICE_USB
        ppp_usb_uninstall();
#endif
        return reg_err;
    }
    s_lost_ip_handler_registered = true;

    net_iface_notify_started(self, s_netif);
    return ESP_OK;
}

/* Begin PPP. Returns as soon as negotiation is started. */
static esp_err_t ppp_ops_connect(net_iface_handle_t self)
{
    if (s_netif == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(self->lock, portMAX_DELAY);
    self->retry_num = 0;
    xSemaphoreGive(self->lock);

#if CONFIG_NET_CONNECT_PPP_DEVICE_USB
    esp_netif_action_start(s_netif, 0, 0, 0);
    esp_netif_action_connected(s_netif, 0, 0, 0);
    net_iface_notify_connected(self);
    return ESP_OK;
#else
    if (s_ppp_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_stop_task = false;

    uint32_t stack_size = CONFIG_NET_CONNECT_PPP_TASK_STACK_SIZE;
    if (self->cfg.ppp.task_stack_size > 0) {
        stack_size = (uint32_t)self->cfg.ppp.task_stack_size;
    }
    if (xTaskCreate(ppp_task, "ppp connect", stack_size, self, 5, &s_ppp_task) != pdPASS) {
        s_ppp_task = NULL;
        ESP_LOGE(TAG, "Failed to create a PPP connection task");
        return ESP_FAIL;
    }
    net_iface_notify_connected(self);
    return ESP_OK;
#endif
}

/* Stop PPP negotiation. The netif object stays until stop(). */
static esp_err_t ppp_ops_disconnect(net_iface_handle_t self)
{
    (void)self;
#if !CONFIG_NET_CONNECT_PPP_DEVICE_USB
    ppp_stop_uart_task();
#endif
    if (s_netif != NULL) {
        esp_netif_action_disconnected(s_netif, 0, 0, 0);
        esp_netif_action_stop(s_netif, 0, 0, 0);
    }
    return ESP_OK;
}

/* Unregister the retry handler, destroy the netif, and release USB if it was used. */
static esp_err_t ppp_ops_stop(net_iface_handle_t self)
{
    (void)self;
    if (s_lost_ip_handler_registered) {
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_PPP_LOST_IP, on_ppp_lost_ip);
        s_lost_ip_handler_registered = false;
    }

#if !CONFIG_NET_CONNECT_PPP_DEVICE_USB
    /* disconnect() already asks the task to leave. This covers stop() from FAILED. */
    ppp_stop_uart_task();
#endif

    if (s_netif != NULL) {
        esp_netif_t *netif = s_netif;
        /* Clear this before destroy so a late USB callback does not use a freed netif. */
        s_netif = NULL;
        esp_netif_action_disconnected(netif, 0, 0, 0);
        esp_netif_action_stop(netif, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_netif_destroy(netif);
    }

#if CONFIG_NET_CONNECT_PPP_DEVICE_USB
    ppp_usb_uninstall();
#endif
    return ESP_OK;
}

/* Driver callbacks used by net_iface_init() and net_ppp_create(). */
const net_iface_ops_t g_net_iface_ppp_ops = {
    .name = "ppp",
    .init = NULL,
    .deinit = NULL,
    .start = ppp_ops_start,
    .stop = ppp_ops_stop,
    .connect = ppp_ops_connect,
    .disconnect = ppp_ops_disconnect,
};

esp_err_t net_ppp_create(const net_ppp_params_t *params, net_iface_handle_t *out_handle)
{
    if (out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;

    net_iface_config_t cfg = { 0 };
    cfg.type = NET_IFACE_TYPE_PPP;
    cfg.desc = NET_CONNECT_NETIF_DESC_PPP;
    cfg.route_prio = NET_CONNECT_PPP_DEFAULT_ROUTE_PRIO;
#if CONFIG_NET_CONNECT_PPP_REQUIRED
    cfg.required = true;
#else
    cfg.required = false;
#endif
    cfg.auto_connect_on_start = true;
    cfg.ip.use_dhcp = true;
#if CONFIG_NET_CONNECT_IPV6
    cfg.ip.enable_ipv6 = true;
#else
    cfg.ip.enable_ipv6 = false;
#endif
    cfg.ppp.max_retry = CONFIG_NET_CONNECT_PPP_CONN_MAX_RETRY;
    cfg.ppp.task_stack_size = CONFIG_NET_CONNECT_PPP_TASK_STACK_SIZE;

    if (params != NULL) {
        if (params->max_retry >= 0) {
            cfg.ppp.max_retry = params->max_retry;
        }
        if (params->task_stack_size > 0) {
            cfg.ppp.task_stack_size = params->task_stack_size;
        }
        if (params->route_prio >= 0) {
            cfg.route_prio = params->route_prio;
        }
        cfg.required = params->required;
        cfg.auto_connect_on_start = params->auto_connect_on_start;
        if (params->ip != NULL) {
            cfg.ip = *params->ip;
        }
    }

    return net_iface_init_with_ops(&cfg, &g_net_iface_ppp_ops, out_handle);
}

#endif /* CONFIG_NET_CONNECT_PPP */
