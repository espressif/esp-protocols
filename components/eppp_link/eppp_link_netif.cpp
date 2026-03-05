/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_check.h"
#include "eppp_link.h"

static const char *TAG = "eppp_link_netif";

extern "C" int eppp_netif_get_num(esp_netif_t *netif)
{
    if (netif == NULL) {
        return -1;
    }
    const char *ifkey = esp_netif_get_ifkey(netif);
    if (strstr(ifkey, "EPPP") == NULL) {
        return -1; // not our netif
    }
    int netif_cnt = ifkey[4] - '0';
    if (netif_cnt < 0 || netif_cnt > 9) {
        ESP_LOGE(TAG, "Unexpected netif key %s", ifkey);
        return -1;
    }
    return  netif_cnt;
}

static bool have_some_eppp_netif_impl(esp_netif_t *netif)
{
    return eppp_netif_get_num(netif) >= 0;
}

// esp_netif_find_predicate_t changes the ctx constness across IDF versions.
// Use traits to discover the exact predicate signature at compile time.
using predicate_t = esp_netif_find_predicate_t;

template <typename> struct predicate_traits;

template <typename R, typename N, typename Ctx> struct predicate_traits<R(*)(N, Ctx)> {
    using netif_t = N;
    using ctx_t   = Ctx;
};

// Adapter that ignores ctx but matches the IDF-specific predicate signature.
template <typename Ctx> static bool have_some_eppp_netif_adapter(esp_netif_t *netif, Ctx ctx)
{
    return have_some_eppp_netif_impl(netif);
}

// Instantiate adapter with the exact ctx type expected by IDF.
static constexpr predicate_t predicate = &have_some_eppp_netif_adapter<typename predicate_traits<predicate_t>::ctx_t>;

extern "C" bool eppp_have_some_netif(void)
{
    return esp_netif_find_if(predicate, NULL) != NULL;
}
