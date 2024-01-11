/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

typedef enum api_id {
    INIT,
    SET_MODE,
    SET_CONFIG,
    START,
    CONNECT,
    GET_MAC
} api_id_t;

struct RpcHeader {
    uint32_t id;
    uint32_t size;
} __attribute((__packed__));

template <typename T> struct RpcData {
    RpcHeader head;
    T value_;
    explicit RpcData(api_id_t id):  head{id, sizeof(T)} {}


    uint8_t *value()
    {
        return (uint8_t *)&value_;
    }

    uint8_t *marshall(T *t, size_t &size)
    {
        size = head.size + sizeof(RpcHeader);
        memcpy(&value_, t, sizeof(T));
        return (uint8_t *)this;
    }


} __attribute((__packed__));

class RpcEngine {
public:
    explicit RpcEngine(esp_tls_t *tls): tls_(tls) {}

    template <typename T> esp_err_t send(api_id_t id, T *t)
    {
//        auto data = std::make_unique<RpcData<T>>(id);
        RpcData<T> req(id);
        size_t size;
        auto buf = req.marshall(t, size);
        ESP_LOGI("rpc", "Sending API id:%d", (int)id);
        ESP_LOG_BUFFER_HEXDUMP("rpc", buf, size, ESP_LOG_VERBOSE);
        int len = esp_tls_conn_write(tls_, buf, size);
        if (len <= 0) {
            ESP_LOGE("rpc", "Failed to write data to the connection");
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    esp_err_t send(api_id_t id) // specialization for (void)
    {
        RpcHeader head = { .id = id, .size = 0 };
        int len = esp_tls_conn_write(tls_, &head, sizeof(head));
        if (len <= 0) {
            ESP_LOGE("rpc", "Failed to write data to the connection");
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    RpcHeader get_header()
    {
        RpcHeader header{};
        int len = esp_tls_conn_read(tls_, (char *)&header, sizeof(header));
        if (len <= 0) {
            ESP_LOGE("prc", "Failed to read data from the connection");
            return {};
        }
        return header;
    }

    template <typename T> T get_payload(api_id_t id, RpcHeader &head)
    {
        RpcData<T> resp(id);
        if (head.id != id || head.size != resp.head.size) {
            ESP_LOGE("prc", "Failed to read data from the connection");
            return {};
        }
        int len = esp_tls_conn_read(tls_, (char *)resp.value(), resp.head.size);
        if (len <= 0) {
            ESP_LOGE("rps", "Failed to read data from the connection");
            return {};
        }
        return resp.value_;
    }

private:
    esp_tls_t *tls_;

};
