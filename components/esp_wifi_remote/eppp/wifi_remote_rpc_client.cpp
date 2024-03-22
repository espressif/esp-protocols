/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include <sys/socket.h>
#include <netdb.h>
#include <memory>
#include "wifi_remote_rpc_impl.hpp"
#include "esp_wifi_remote.h"

#define PORT 3333

using namespace eppp_rpc;
static const char *TAG = "rpc_client";

esp_err_t RpcEngine::init_client()
{
    esp_err_t ret = ESP_OK;
    const char host[] = "192.168.11.1";
    int port = 3333;
    esp_tls_cfg_t cfg = {};
    cfg.skip_common_name = true;

    tls_ = esp_tls_init();
    if (!tls_) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        ret = ESP_FAIL;
        goto exit;
    }
    if (esp_tls_conn_new_sync(host, strlen(host), port, &cfg, tls_) <= 0) {
        ESP_LOGE(TAG, "Failed to open a new connection");
        ret = ESP_FAIL;
        goto exit;
    }
    return ESP_OK;
exit:
    esp_tls_conn_destroy(tls_);
    tls_ = nullptr;
    return ret;
}
