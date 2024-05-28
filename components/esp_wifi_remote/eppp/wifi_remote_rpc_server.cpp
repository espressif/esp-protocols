/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <netdb.h>
#include <memory>
#include <cerrno>
#include <sys/socket.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "wifi_remote_rpc_impl.hpp"
#include "eppp_link.h"
#include "wifi_remote_rpc_params.h"
#include "lwip/apps/snmp.h"

extern "C" esp_netif_t *wifi_remote_eppp_init(eppp_type_t role);

namespace eppp_rpc {

namespace server {
const char *TAG = "rpc_server";

const unsigned char ca_crt[] = "-----BEGIN CERTIFICATE-----\n" CONFIG_ESP_WIFI_REMOTE_EPPP_CLIENT_CA "\n-----END CERTIFICATE-----";
const unsigned char crt[] = "-----BEGIN CERTIFICATE-----\n" CONFIG_ESP_WIFI_REMOTE_EPPP_SERVER_CRT "\n-----END CERTIFICATE-----";
const unsigned char key[] = "-----BEGIN PRIVATE KEY-----\n" CONFIG_ESP_WIFI_REMOTE_EPPP_SERVER_KEY "\n-----END PRIVATE KEY-----";
// TODO: Add option to supply keys and certs via a global symbol (file)

}

using namespace server;

class RpcInstance {
public:
    RpcEngine rpc{role::SERVER};
    int sock{-1};

    esp_err_t init()
    {
        ESP_RETURN_ON_FALSE(netif = wifi_remote_eppp_init(EPPP_SERVER), ESP_FAIL, TAG, "Failed to init EPPP connection");
        ESP_RETURN_ON_ERROR(start_server(), TAG, "Failed to start RPC server");
        ESP_RETURN_ON_ERROR(rpc.init(), TAG, "Failed to init RPC engine");
        ESP_RETURN_ON_ERROR(esp_netif_napt_enable(netif), TAG, "Failed to enable NAPT");
        ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, handler, this), TAG, "Failed to register event");
        ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, handler, this), TAG, "Failed to register event");
        return xTaskCreate(task, "server", 8192, this, 5, nullptr) == pdTRUE ? ESP_OK : ESP_FAIL;
    }

private:
    esp_netif_t *netif{nullptr};
    static void task(void *ctx)
    {
        auto instance = static_cast<RpcInstance *>(ctx);
        while (instance->perform() == ESP_OK) {}
        esp_restart();
    }
    esp_err_t start_server()
    {
        struct sockaddr_in dest_addr = {};
        int ret;
        int opt = 1;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(rpc_port);
        int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        ESP_RETURN_ON_FALSE(listen_sock >= 0, ESP_FAIL, TAG, "Failed to create listening socket");
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        ret = bind(listen_sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
        ESP_RETURN_ON_FALSE(ret == 0, ESP_FAIL, TAG, "Failed to bind the listening socket");
        ret = listen(listen_sock, 1);
        ESP_RETURN_ON_FALSE(ret == 0, ESP_FAIL, TAG, "Failed to start listening");
        struct sockaddr_storage source_addr {};
        socklen_t addr_len = sizeof(source_addr);
        sock = accept(listen_sock, (struct sockaddr *) &source_addr, &addr_len);
        ESP_RETURN_ON_FALSE(sock >= 0, ESP_FAIL, TAG, "Failed to accept connections: errno %d", errno);
        ESP_LOGI(TAG, "Socket accepted on: %s", inet_ntoa(((struct sockaddr_in *) &source_addr)->sin_addr));
        return ESP_OK;
    }
    esp_err_t wifi_event(int32_t id)
    {
        ESP_LOGI(TAG, "Received WIFI event %" PRIi32, id);
        ESP_RETURN_ON_ERROR(rpc.send(api_id::WIFI_EVENT, &id), TAG, "Failed to marshall WiFi event");
        return ESP_OK;
    }
    esp_err_t ip_event(int32_t id, ip_event_got_ip_t *ip_data)
    {
        ESP_LOGI(TAG, "Received IP event %" PRIi32, id);
        esp_wifi_remote_eppp_ip_event ip_event{};
        ip_event.id = id;
        if (ip_data->esp_netif) {
            // marshall additional data, only if netif available
            ESP_RETURN_ON_ERROR(esp_netif_get_dns_info(ip_data->esp_netif, ESP_NETIF_DNS_MAIN, &ip_event.dns), TAG, "Failed to get DNS info");
            ESP_LOGI(TAG, "Main DNS:" IPSTR, IP2STR(&ip_event.dns.ip.u_addr.ip4));
            memcpy(&ip_event.wifi_ip, &ip_data->ip_info, sizeof(ip_event.wifi_ip));
            ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(netif, &ip_event.ppp_ip), TAG, "Failed to get IP info");
            ESP_LOGI(TAG, "IP address:" IPSTR, IP2STR(&ip_data->ip_info.ip));
        }
        ESP_RETURN_ON_ERROR(rpc.send(api_id::IP_EVENT, &ip_event), TAG, "Failed to marshal IP event");
        return ESP_OK;
    }
    static void handler(void *ctx, esp_event_base_t base, int32_t id, void *data)
    {
        auto instance = static_cast<RpcInstance *>(ctx);
        if (base == WIFI_EVENT) {
            instance->wifi_event(id);
        } else if (base == IP_EVENT) {
            auto *ip_data = (ip_event_got_ip_t *)data;
            instance->ip_event(id, ip_data);
        }
    }
    esp_err_t perform()
    {
        auto header = rpc.get_header();
        ESP_LOGI(TAG, "Received header id %d", (int) header.id);

        switch (header.id) {
        case api_id::SET_MODE: {
            auto req = rpc.get_payload<wifi_mode_t>(api_id::SET_MODE, header);
            auto ret = esp_wifi_set_mode(req);
            if (rpc.send(api_id::SET_MODE, &ret) != ESP_OK) {
                return ESP_FAIL;
            }
            break;
        }
        case api_id::INIT: {
            auto req = rpc.get_payload<wifi_init_config_t>(api_id::INIT, header);
            req.osi_funcs = &g_wifi_osi_funcs;
            req.wpa_crypto_funcs = g_wifi_default_wpa_crypto_funcs;
            auto ret = esp_wifi_init(&req);
            if (rpc.send(api_id::INIT, &ret) != ESP_OK) {
                return ESP_FAIL;
            }
            break;
        }
        case api_id::SET_CONFIG: {
            auto req = rpc.get_payload<esp_wifi_remote_config>(api_id::SET_CONFIG, header);
            auto ret = esp_wifi_set_config(req.interface, &req.conf);
            if (rpc.send(api_id::SET_CONFIG, &ret) != ESP_OK) {
                return ESP_FAIL;
            }
            break;
        }
        case api_id::START: {
            if (header.size != 0) {
                return ESP_FAIL;
            }

            auto ret = esp_wifi_start();
            if (rpc.send(api_id::START, &ret) != ESP_OK) {
                return ESP_FAIL;
            }
            break;
        }
        case api_id::CONNECT: {
            if (header.size != 0) {
                return ESP_FAIL;
            }

            auto ret = esp_wifi_connect();
            if (rpc.send(api_id::CONNECT, &ret) != ESP_OK) {
                return ESP_FAIL;
            }
            break;
        }
        case api_id::DISCONNECT: {
            if (header.size != 0) {
                return ESP_FAIL;
            }

            auto ret = esp_wifi_disconnect();
            if (rpc.send(api_id::DISCONNECT, &ret) != ESP_OK) {
                return ESP_FAIL;
            }
            break;
        }
        case api_id::DEINIT: {
            if (header.size != 0) {
                return ESP_FAIL;
            }

            auto ret = esp_wifi_deinit();
            if (rpc.send(api_id::DEINIT, &ret) != ESP_OK) {
                return ESP_FAIL;
            }
            break;
        }
        case api_id::SET_STORAGE: {
            auto req = rpc.get_payload<wifi_storage_t>(api_id::SET_STORAGE, header);
            auto ret = esp_wifi_set_storage(req);
            if (rpc.send(api_id::SET_STORAGE, &ret) != ESP_OK) {
                return ESP_FAIL;
            }
            break;
        }
        case api_id::GET_MAC: {
            auto req = rpc.get_payload<wifi_interface_t>(api_id::GET_MAC, header);
            esp_wifi_remote_mac_t resp = {};
            resp.err = esp_wifi_get_mac(req, resp.mac);
            if (rpc.send(api_id::GET_MAC, &resp) != ESP_OK) {
                return ESP_FAIL;
            }
            break;
        }
        default:
            return ESP_FAIL;
        }
        return ESP_OK;
    }
};


namespace server {
constinit RpcInstance instance;
}

RpcInstance *RpcEngine::init_server()
{
    esp_tls_cfg_server_t cfg = {};
    cfg.cacert_buf = server::ca_crt;
    cfg.cacert_bytes = sizeof(server::ca_crt);
    cfg.servercert_buf = server::crt;
    cfg.servercert_bytes = sizeof(server::crt);
    cfg.serverkey_buf = server::key;
    cfg.serverkey_bytes = sizeof(server::key);

    ESP_RETURN_ON_FALSE(tls_ = esp_tls_init(), nullptr, TAG, "Failed to create ESP-TLS instance");
    ESP_RETURN_ON_FALSE(esp_tls_server_session_create(&cfg, server::instance.sock, tls_) == ESP_OK, nullptr, TAG, "Failed to create TLS session");
    return &server::instance;
}

}   // namespace eppp_rpc

using namespace eppp_rpc;

extern "C" esp_err_t server_init(void)
{
    return server::instance.init();
}
