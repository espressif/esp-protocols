/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "eppp_link.h"
#include "eppp_transport.h"
#include "eppp_transport_sdio.h"
#include "eppp_sdio.h"

#define TAG "eppp_sdio"

struct eppp_sdio {
    struct eppp_handle parent;
    bool is_host;
};

esp_err_t eppp_sdio_host_tx(void *h, void *buffer, size_t len);
esp_err_t eppp_sdio_host_rx(esp_netif_t *netif);
esp_err_t eppp_sdio_slave_rx(esp_netif_t *netif);
esp_err_t eppp_sdio_slave_tx(void *h, void *buffer, size_t len);
esp_err_t eppp_sdio_host_init(struct eppp_config_sdio_s *config);
esp_err_t eppp_sdio_slave_init(struct eppp_config_sdio_s *config);
void eppp_sdio_slave_deinit(void);
void eppp_sdio_host_deinit(void);

esp_err_t eppp_perform(esp_netif_t *netif)
{
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    if (h->stop) {
        return ESP_ERR_TIMEOUT;
    }
    struct eppp_sdio *handle = __containerof(h, struct eppp_sdio, parent);;
    if (handle->is_host) {
        return eppp_sdio_host_rx(netif);
    } else {
        return eppp_sdio_slave_rx(netif);
    }
}

static esp_err_t post_attach(esp_netif_t *esp_netif, void *args)
{
    eppp_transport_handle_t h = (eppp_transport_handle_t)args;
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "Transport handle cannot be null");
    struct eppp_sdio *sdio = __containerof(h, struct eppp_sdio, parent);
    h->base.netif = esp_netif;

    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle =  h,
        .transmit = sdio->is_host ? eppp_sdio_host_tx : eppp_sdio_slave_tx,
    };

    ESP_RETURN_ON_ERROR(esp_netif_set_driver_config(esp_netif, &driver_ifconfig), TAG, "Failed to set driver config");
    ESP_LOGI(TAG, "EPPP SDIO transport attached to EPPP netif %s", esp_netif_get_desc(esp_netif));
    return ESP_OK;
}

eppp_transport_handle_t eppp_sdio_init(struct eppp_config_sdio_s *config)
{
    __attribute__((unused)) esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(config, NULL, TAG, "Config cannot be null");
    struct eppp_sdio *h = calloc(1, sizeof(struct eppp_sdio));
    ESP_RETURN_ON_FALSE(h, NULL, TAG, "Failed to allocate eppp_handle");
#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
    h->parent.channel_tx = eppp_sdio_transmit_channel;
#endif
    h->parent.base.post_attach = post_attach;
    h->is_host = config->is_host;
    esp_err_t (*init_fn)(struct eppp_config_sdio_s * eppp_config) = h->is_host ? eppp_sdio_host_init : eppp_sdio_slave_init;
    ESP_GOTO_ON_ERROR(init_fn(config), err, TAG, "Failed to init SDIO");
    return &h->parent;
err:
    free(h);
    return NULL;
}

void eppp_sdio_deinit(eppp_transport_handle_t h)
{
    struct eppp_sdio *sdio = __containerof(h, struct eppp_sdio, parent);
    if (sdio->is_host) {
        eppp_sdio_host_deinit();
    } else {
        eppp_sdio_slave_deinit();
    }
}
