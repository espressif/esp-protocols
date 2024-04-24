/* Copyright 2024 Tenera Care
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "esp_idf_version.h"
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 0)
    // Add missing includes to esp_netif_types.h
    // https://github.com/espressif/esp-idf/commit/822129e234aaedf86e76fe92ab9b49c5f0a612e0
    #include "esp_err.h"
    #include "esp_event_base.h"
    #include "esp_netif_ip_addr.h"

    #include <stdbool.h>
    #include <stdint.h>
#endif

#include "esp_netif_types.h"
#include "nimble/ble.h"

void lowpan6_ble_netif_up(esp_netif_t* esp_netif, ble_addr_t* peer_addr, ble_addr_t* our_addr);

void lowpan6_ble_netif_down(esp_netif_t* netif);
