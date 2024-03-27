/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "wifi_remote_rpc_impl.hpp"
#include "eppp_link.h"

using namespace eppp_rpc;

static RpcEngine s_rpc(eppp_rpc::role::CLIENT);
static esp_netif_t *s_ppp_netif;
static const char *TAG = "rpc_impl";

extern "C" esp_netif_t *wifi_remote_eppp_init(eppp_type_t role);

extern "C" esp_err_t esp_wifi_remote_init(const wifi_init_config_t *config)
{
    s_ppp_netif = wifi_remote_eppp_init(EPPP_CLIENT);
    if (s_ppp_netif == nullptr) {
        return ESP_FAIL;
    }
    s_rpc.init();
    if (s_rpc.send(INIT, config) != ESP_OK) {
        return ESP_FAIL;
    }
    auto header = s_rpc.get_header();
    return s_rpc.get_payload<esp_err_t>(INIT, header);
}

#if 0


union wifi_config_t2 {
        wifi_ap_config_t  ap;  /**< configuration of AP */
        wifi_sta_config_t sta; /**< configuration of STA */
        wifi_nan_config_t nan; /**< configuration of NAN */
    void marshall(int type) {

}
} ;

struct marshaller {
    template<typename T> uint8_t *marshall(uint8_t *ptr, T *t)
    {
        memcpy(ptr, t, sizeof(T));
        ptr += sizeof(T);
        return ptr;
    }
};

struct esp_wifi_remote_config {
//    struct wifi_interface_t {
//        wifi_interface_t *ptr;
//    } p1;
    struct wifi_interface_t : public marshaller {
        ::wifi_interface_t *interface;
        uint8_t *marshall(uint8_t *ptr)
        {
            return marshaller::marshall(ptr, (uint32_t *)interface);
        }
//            memcpy(ptr, interface, sizeof(*interface)); return ptr+sizeof(*interface); }
    } wifi_interface_t;

//    wifi_config_t2 *conf;
    struct wifi_config_t2 {
        ::wifi_config_t2 *config;
        struct wifi_ap_config_t {
            ::wifi_ap_config_t *ap;
            struct wifi_pmf_config_t {
                ::wifi_pmf_config_t *pmf_cfg;
                uint8_t *marshall(uint8_t *ptr)
                {
                    if (pmf_cfg) {
                        *ptr = pmf_cfg->capable;
                        ptr++;
                        ptr++;
                    } return ptr;
                }
            } wifi_pmf_config_t;
            uint8_t *marshall(uint8_t *ptr)
            {
                if (ap) {
                    marshar_true();
                    *ptr = ap->channel; ptr++; // ...
                    ptr = wifi_pmf_config_t.marshall(ptr);
                } else {
                    marshall_false();
                }
                return ptr;
            }
        } wifi_ap_config_t;
        uint8_t *marshall(uint8_t *ptr)
        {
            // nothing
            ptr = wifi_ap_config_t.marshall(ptr);
            // ptr = wifi_sta_config_t.marshall(ptr);
            return ptr;
        }
    } wifi_config_t2_;

    uint8_t *marshall(uint8_t *ptr)
    {
        ptr = wifi_interface_t.marshall(ptr);
        ptr = wifi_config_t2_.marshall(ptr);
        return ptr;
    }
};
#endif

#if 0

extern "C" esp_err_t esp_wifi_remote_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
    esp_wifi_remote_config params =
    esp_wifi_remote_config params = {
        .wifi_interface_t = { .interface = &interface },
        .wifi_config_t2_ = {
            .config = nullptr,
            .wifi_ap_config_t = {
                .ap = &conf->ap,
                .wifi_pmf_config_t = {
                    .pmf_cfg = &conf->ap.pmf_cfg,
                }
            }
        }
    }; //  { .interface = &interface, .conf = {} };

    esp_wifi_remote_config params2 = {
        .wifi_interface_t = { .interface = interface },
        .wifi_config_t2_ = {
            .config = conf,
            .wifi_ap_config_t = {
                .ap = true,
                .wifi_pmf_config_t = {
                    .pmf_cfg = true,
                }
            }
            .wifi_sta_config_t = {
                .ap = false,
                .wifi_pmf_config_t = {
                    .pmf_cfg = false,
                }
            }

        }
    }; //  { .interface = &interface, .conf = {} };

    params.wifi_interface_t.interface = &interface;
    params.wifi_config_t2_.config = nullptr;
    params.wifi_config_t2_.wifi_ap_config_t.ap = &conf->ap;
    params.wifi_config_t2_.wifi_ap_config_t.wifi_pmf_config_t.pmf_cfg = &conf->ap.pmf_cfg;
    params.marshall(nullptr);
//    params.wifi_config_t2.ap = nullptr;
//    params.marshall(2);
//    memcpy(&params.conf, conf, sizeof(wifi_config_t));
    if (s_rpc.send(SET_CONFIG, &params) != ESP_OK) {
        return ESP_FAIL;
    }
    auto header = s_rpc.get_header();
    return s_rpc.get_payload<esp_err_t>(SET_CONFIG, header);
}
#endif
