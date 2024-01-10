/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
//
// Created by david on 1/10/24.
//
#include "esp_log.h"
#include "esp_tls.h"
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <memory>
#include "esp_wifi.h"
#include "rpc.hpp"
#include "esp_wifi_remote.h"

#define PORT 3333
static const char *TAG = "server";
static esp_tls_t *tls;

const unsigned char servercert[] = "-----BEGIN CERTIFICATE-----\n"
                                   "MIIDKzCCAhOgAwIBAgIUBxM3WJf2bP12kAfqhmhhjZWv0ukwDQYJKoZIhvcNAQEL\n"
                                   "BQAwJTEjMCEGA1UEAwwaRVNQMzIgSFRUUFMgc2VydmVyIGV4YW1wbGUwHhcNMTgx\n"
                                   "MDE3MTEzMjU3WhcNMjgxMDE0MTEzMjU3WjAlMSMwIQYDVQQDDBpFU1AzMiBIVFRQ\n"
                                   "UyBzZXJ2ZXIgZXhhbXBsZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"
                                   "ALBint6nP77RCQcmKgwPtTsGK0uClxg+LwKJ3WXuye3oqnnjqJCwMEneXzGdG09T\n"
                                   "sA0SyNPwrEgebLCH80an3gWU4pHDdqGHfJQa2jBL290e/5L5MB+6PTs2NKcojK/k\n"
                                   "qcZkn58MWXhDW1NpAnJtjVniK2Ksvr/YIYSbyD+JiEs0MGxEx+kOl9d7hRHJaIzd\n"
                                   "GF/vO2pl295v1qXekAlkgNMtYIVAjUy9CMpqaQBCQRL+BmPSJRkXBsYk8GPnieS4\n"
                                   "sUsp53DsNvCCtWDT6fd9D1v+BB6nDk/FCPKhtjYOwOAZlX4wWNSZpRNr5dfrxKsb\n"
                                   "jAn4PCuR2akdF4G8WLUeDWECAwEAAaNTMFEwHQYDVR0OBBYEFMnmdJKOEepXrHI/\n"
                                   "ivM6mVqJgAX8MB8GA1UdIwQYMBaAFMnmdJKOEepXrHI/ivM6mVqJgAX8MA8GA1Ud\n"
                                   "EwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBADiXIGEkSsN0SLSfCF1VNWO3\n"
                                   "emBurfOcDq4EGEaxRKAU0814VEmU87btIDx80+z5Dbf+GGHCPrY7odIkxGNn0DJY\n"
                                   "W1WcF+DOcbiWoUN6DTkAML0SMnp8aGj9ffx3x+qoggT+vGdWVVA4pgwqZT7Ybntx\n"
                                   "bkzcNFW0sqmCv4IN1t4w6L0A87ZwsNwVpre/j6uyBw7s8YoJHDLRFT6g7qgn0tcN\n"
                                   "ZufhNISvgWCVJQy/SZjNBHSpnIdCUSJAeTY2mkM4sGxY0Widk8LnjydxZUSxC3Nl\n"
                                   "hb6pnMh3jRq4h0+5CZielA4/a+TdrNPv/qok67ot/XJdY3qHCCd8O2b14OVq9jo=\n"
                                   "-----END CERTIFICATE-----";
const unsigned char prvtkey[] = "-----BEGIN PRIVATE KEY-----\n"
                                "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCwYp7epz++0QkH\n"
                                "JioMD7U7BitLgpcYPi8Cid1l7snt6Kp546iQsDBJ3l8xnRtPU7ANEsjT8KxIHmyw\n"
                                "h/NGp94FlOKRw3ahh3yUGtowS9vdHv+S+TAfuj07NjSnKIyv5KnGZJ+fDFl4Q1tT\n"
                                "aQJybY1Z4itirL6/2CGEm8g/iYhLNDBsRMfpDpfXe4URyWiM3Rhf7ztqZdveb9al\n"
                                "3pAJZIDTLWCFQI1MvQjKamkAQkES/gZj0iUZFwbGJPBj54nkuLFLKedw7DbwgrVg\n"
                                "0+n3fQ9b/gQepw5PxQjyobY2DsDgGZV+MFjUmaUTa+XX68SrG4wJ+DwrkdmpHReB\n"
                                "vFi1Hg1hAgMBAAECggEAaTCnZkl/7qBjLexIryC/CBBJyaJ70W1kQ7NMYfniWwui\n"
                                "f0aRxJgOdD81rjTvkINsPp+xPRQO6oOadjzdjImYEuQTqrJTEUnntbu924eh+2D9\n"
                                "Mf2CAanj0mglRnscS9mmljZ0KzoGMX6Z/EhnuS40WiJTlWlH6MlQU/FDnwC6U34y\n"
                                "JKy6/jGryfsx+kGU/NRvKSru6JYJWt5v7sOrymHWD62IT59h3blOiP8GMtYKeQlX\n"
                                "49om9Mo1VTIFASY3lrxmexbY+6FG8YO+tfIe0tTAiGrkb9Pz6tYbaj9FjEWOv4Vc\n"
                                "+3VMBUVdGJjgqvE8fx+/+mHo4Rg69BUPfPSrpEg7sQKBgQDlL85G04VZgrNZgOx6\n"
                                "pTlCCl/NkfNb1OYa0BELqWINoWaWQHnm6lX8YjrUjwRpBF5s7mFhguFjUjp/NW6D\n"
                                "0EEg5BmO0ePJ3dLKSeOA7gMo7y7kAcD/YGToqAaGljkBI+IAWK5Su5yldrECTQKG\n"
                                "YnMKyQ1MWUfCYEwHtPvFvE5aPwKBgQDFBWXekpxHIvt/B41Cl/TftAzE7/f58JjV\n"
                                "MFo/JCh9TDcH6N5TMTRS1/iQrv5M6kJSSrHnq8pqDXOwfHLwxetpk9tr937VRzoL\n"
                                "CuG1Ar7c1AO6ujNnAEmUVC2DppL/ck5mRPWK/kgLwZSaNcZf8sydRgphsW1ogJin\n"
                                "7g0nGbFwXwKBgQCPoZY07Pr1TeP4g8OwWTu5F6dSvdU2CAbtZthH5q98u1n/cAj1\n"
                                "noak1Srpa3foGMTUn9CHu+5kwHPIpUPNeAZZBpq91uxa5pnkDMp3UrLIRJ2uZyr8\n"
                                "4PxcknEEh8DR5hsM/IbDcrCJQglM19ZtQeW3LKkY4BsIxjDf45ymH407IQKBgE/g\n"
                                "Ul6cPfOxQRlNLH4VMVgInSyyxWx1mODFy7DRrgCuh5kTVh+QUVBM8x9lcwAn8V9/\n"
                                "nQT55wR8E603pznqY/jX0xvAqZE6YVPcw4kpZcwNwL1RhEl8GliikBlRzUL3SsW3\n"
                                "q30AfqEViHPE3XpE66PPo6Hb1ymJCVr77iUuC3wtAoGBAIBrOGunv1qZMfqmwAY2\n"
                                "lxlzRgxgSiaev0lTNxDzZkmU/u3dgdTwJ5DDANqPwJc6b8SGYTp9rQ0mbgVHnhIB\n"
                                "jcJQBQkTfq6Z0H6OoTVi7dPs3ibQJFrtkoyvYAbyk36quBmNRjVh6rc8468bhXYr\n"
                                "v/t+MeGJP/0Zw8v/X2CFll96\n"
                                "-----END PRIVATE KEY-----";


static esp_err_t perform()
{
//    RpcHeader header{};
//    int len = esp_tls_conn_read(tls, (char *)&header, sizeof(header));
//    if (len <= 0) {
//        ESP_LOGE(TAG, "Failed to read data from the connection");
//        return ESP_FAIL;
//    }
    RpcEngine rpc(tls);

    auto header = rpc.get_header();

//    auto data = std::make_unique<RpcData<wifi_mode_t>>(SET_MODE);

    switch (header.id) {
    case SET_MODE: {
        auto req = rpc.get_payload<wifi_mode_t>(SET_MODE, header);
//            auto data = std::make_unique<RpcData<wifi_mode_t>>(SET_MODE);
//            if (data->head.size != header.size) {
//                ESP_LOGE(TAG, "Data size mismatch problem! %d expected, %d given", (int)data->head.size, (int)header.size);
//                return ESP_FAIL;
//            }
//            len = esp_tls_conn_read(tls, (char *)data->value(), data->head.size);
//            if (len <= 0) {
//                ESP_LOGE(TAG, "Failed to read data from the connection");
//                return ESP_FAIL;
//            }
//            auto resp = std::make_unique<RpcData<esp_err_t>>(SET_MODE);
        auto ret = esp_wifi_set_mode(req);
        if (rpc.send(SET_MODE, &ret) != ESP_OK) {
            return ESP_FAIL;
        }
        break;
//
//            ESP_LOGI(TAG, "esp_wifi_set_mode() returned %x", ret);
//            size_t size;
//            auto buf = resp->marshall(&ret, size);
////
////            resp->value_ = esp_wifi_set_mode(data->value_);
////            ESP_LOGE(TAG, "size=%d", (int)data->size_);
//            ESP_LOG_BUFFER_HEXDUMP(TAG, buf, size, ESP_LOG_INFO);
//            len = esp_tls_conn_write(tls, buf, size);
//            if (len <= 0) {
//                ESP_LOGE(TAG, "Failed to write data to the connection");
//                return ESP_FAIL;
//            }

    }
    case INIT: {
        auto req = rpc.get_payload<wifi_init_config_t>(INIT, header);
        ESP_LOG_BUFFER_HEXDUMP("cfg", &req, sizeof(req), ESP_LOG_WARN);
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_LOG_BUFFER_HEXDUMP("cfg", &cfg, sizeof(cfg), ESP_LOG_WARN);

        auto ret = esp_wifi_init(&req);
        if (rpc.send(INIT, &ret) != ESP_OK) {
            return ESP_FAIL;
        }
        break;
    }
    case SET_CONFIG: {
        auto req = rpc.get_payload<esp_wifi_remote_config>(SET_CONFIG, header);
        auto ret = esp_wifi_set_config(req.interface, &req.conf);
        if (rpc.send(SET_CONFIG, &ret) != ESP_OK) {
            return ESP_FAIL;
        }
        break;
    }
    case START: {
        if (header.size != 0) {
            return ESP_FAIL;
        }
        auto ret = esp_wifi_start();
        if (rpc.send(START, &ret) != ESP_OK) {
            return ESP_FAIL;
        }
        break;
    }
    case CONNECT: {
        if (header.size != 0) {
            return ESP_FAIL;
        }
        auto ret = esp_wifi_connect();
        if (rpc.send(CONNECT, &ret) != ESP_OK) {
            return ESP_FAIL;
        }
        break;
    }


    }
    return ESP_OK;

//    ESP_LOGW(TAG, "Data from the connection (size=%d)", len);
//    ESP_LOG_BUFFER_HEXDUMP(TAG, buf, len, ESP_LOG_WARN);
//    len = esp_tls_conn_write(tls,"ZDAR",4);
//    if (len <= 0) {
//        ESP_LOGE(TAG, "Failed to write data to the connection");
//        goto cleanup;
//    }

}

static void server(void *ctx)
{
    char buf[512];

    struct sockaddr_in dest_addr = {};
    int ret;
    int opt = 1;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        printf("Unable to create socket: errno %d", errno);
        return;
    }
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ret = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (ret != 0) {
        printf("Socket unable to bind: errno %d", errno);
        return;
    }

    ret = listen(listen_sock, 1);
    if (ret != 0) {
        printf("Error occurred during listen: errno %d", errno);
        return;
    }
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    if (sock < 0) {
        printf("Unable to accept connection: errno %d", errno);
        return;
    }
    printf("Socket accepted ip address: %s\n", inet_ntoa(((struct sockaddr_in *)&source_addr)->sin_addr));


    esp_tls_cfg_server_t cfg = {};
    cfg.servercert_buf = servercert;
    cfg.servercert_bytes = sizeof(servercert);
    cfg.serverkey_buf = prvtkey;
    cfg.serverkey_bytes = sizeof(prvtkey);



    tls = esp_tls_init();
    if (!tls) {
        goto exit;
    }
    ESP_LOGI(TAG, "performing session handshake");
    ret = esp_tls_server_session_create(&cfg, sock, tls);
    if (ret != 0) {
        ESP_LOGE(TAG, "esp_tls_create_server_session failed");
        goto exit;
    }
    ESP_LOGI(TAG, "Secure socket open");
    memset(buf, 0x00, sizeof(buf));
    while (perform() == ESP_OK) {}


    esp_tls_server_session_delete(tls);
exit:
    vTaskDelete(NULL);

}

extern "C" esp_err_t server_init(void)
{
    xTaskCreate(&server, "server", 8192, NULL, 5, NULL);
    return ESP_OK;
}
