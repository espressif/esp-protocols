/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <memory>
#include "rpc.hpp"
#include "esp_wifi_remote.h"

#define PORT 3333


static const char *TAG = "client";
esp_tls_t *tls;



extern "C" esp_err_t client_init(void)
{
    esp_err_t ret = ESP_OK;
//    char buf[512];
//    int len;
    const char host[] = "192.168.11.1";
    int port = 3333;
    esp_tls_cfg_t cfg = {};
    cfg.skip_common_name = true;

    tls = esp_tls_init();
    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        ret = ESP_FAIL;
        goto exit;
    }
    if (esp_tls_conn_new_sync(host, strlen(host), port, &cfg, tls) <= 0) {
        ESP_LOGE(TAG, "Failed to open a new connection");
        ret = ESP_FAIL;
        goto exit;
    }

//
//
//    len = esp_tls_conn_write(tls,"AHOJ",4);
//    if (len <= 0) {
//        ESP_LOGE(TAG, "Failed to write data to the connection");
//        goto cleanup;
//    }
//    memset(buf, 0x00, sizeof(buf));
//    len = esp_tls_conn_read(tls, (char *)buf, len);
//    if (len <= 0) {
//        ESP_LOGE(TAG, "Failed to read data from the connection");
//        goto cleanup;
//    }
//    ESP_LOGI(TAG, "Data from the connection (size=%d)", len);
//    ESP_LOG_BUFFER_HEXDUMP(TAG, buf, len, ESP_LOG_INFO);

//cleanup:
//    esp_tls_conn_destroy(tls);
exit:
    return ret;
}

extern "C" int client_deinit(void)
{
    return esp_tls_conn_destroy(tls);
}


//template <typename T> uint8_t *marshall(uint32_t id, T *t, size_t& len)
//{
//    len = sizeof(RpcData<T>);
//    auto *data = new RpcData<T>();
//    data->id = id;
//    data->size = sizeof(RpcData<T>);
//    memcpy(&data->value, t, sizeof(T));
//    return data->get();
//}


extern "C" esp_err_t esp_wifi_remote_set_mode(wifi_mode_t mode)
{
//    auto data = std::make_unique<RpcData<wifi_mode_t>>(SET_MODE);
//    size_t size;
//    auto buf = data->marshall(&mode, size);
////    auto buf = marshall(SET_MODE, &mode, size);
////    ESP_LOGE(TAG, "size=%d", (int)data->size_);
//    ESP_LOG_BUFFER_HEXDUMP(TAG, buf, size, ESP_LOG_INFO);
//    int len = esp_tls_conn_write(tls, buf, size);
//    if (len <= 0) {
//        ESP_LOGE(TAG, "Failed to write data to the connection");
//        return ESP_FAIL;
//    }
    RpcEngine rpc(tls);

    if (rpc.send(SET_MODE, &mode) != ESP_OK) {
        return ESP_FAIL;
    }

//    RpcHeader header{};

    auto header = rpc.get_header();
    auto resp = rpc.get_payload<esp_err_t>(SET_MODE, header);

//    int len = esp_tls_conn_read(tls, (char *)&header, sizeof(header));
//    if (len <= 0) {
//        ESP_LOGE(TAG, "Failed to read data from the connection");
//        return ESP_FAIL;
//    }

//    auto resp = std::make_unique<RpcData<esp_err_t>>(SET_MODE);
//    if (resp->head.size != header.size || resp->head.id != header.id) {
//        ESP_LOGE(TAG, "Data size mismatch problem! %d expected, %d given", (int)resp->head.size, (int)header.size);
//        ESP_LOGE(TAG, "API ID mismatch problem! %d expected, %d given", (int)resp->head.id, (int)header.id);
//        return ESP_FAIL;
//    }
//    int len = esp_tls_conn_read(tls, (char *)resp->value(), resp->head.size);
//    if (len <= 0) {
//        ESP_LOGE(TAG, "Failed to read data from the connection");
//        return ESP_FAIL;
//    }
//    ESP_LOG_BUFFER_HEXDUMP(TAG, resp->value(), data->head.size, ESP_LOG_INFO);
//    ESP_LOGE(TAG, "value_ is returned %x", *(int*)(resp->value()));

    return resp;
}


extern "C" esp_err_t esp_wifi_remote_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
    RpcEngine rpc(tls);
    esp_wifi_remote_config params = { .interface = interface, .conf = {} };
    memcpy(&params.conf, conf, sizeof(wifi_config_t));
    if (rpc.send(SET_CONFIG, &params) != ESP_OK) {
        return ESP_FAIL;
    }
    auto header = rpc.get_header();
    return rpc.get_payload<esp_err_t>(SET_CONFIG, header);
}

extern "C" esp_err_t esp_wifi_remote_init(wifi_init_config_t *config)
{
    RpcEngine rpc(tls);

    if (rpc.send(INIT, config) != ESP_OK) {
        return ESP_FAIL;
    }
    auto header = rpc.get_header();
    return rpc.get_payload<esp_err_t>(INIT, header);
}

extern "C" esp_err_t esp_wifi_remote_start(void)
{
    RpcEngine rpc(tls);

    if (rpc.send(START) != ESP_OK) {
        return ESP_FAIL;
    }
    auto header = rpc.get_header();
    return rpc.get_payload<esp_err_t>(START, header);
}

extern "C" esp_err_t esp_wifi_remote_connect(void)
{
    RpcEngine rpc(tls);

    if (rpc.send(CONNECT) != ESP_OK) {
        return ESP_FAIL;
    }
    auto header = rpc.get_header();
    return rpc.get_payload<esp_err_t>(CONNECT, header);
}

extern "C" esp_err_t esp_wifi_remote_get_mac(wifi_interface_t ifx, uint8_t mac[6])
{
    RpcEngine rpc(tls);

    if (rpc.send(GET_MAC, &ifx) != ESP_OK) {
        return ESP_FAIL;
    }
    auto header = rpc.get_header();
    auto ret = rpc.get_payload<esp_wifi_remote_mac_t>(GET_MAC, header);
    ESP_LOG_BUFFER_HEXDUMP("MAC", ret.mac, 6, ESP_LOG_INFO);

    memcpy(mac, ret.mac, 6);
    return ret.err;

}
