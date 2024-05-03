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
#include "lwip/ip6_addr.h"
#include "nimble/ble.h"

/** Bring up the netif upon connection.
 *
 * @param[in] esp_netif The netif to mark up.
 * @param[in] peer_addr The BLE address of the peer we've connected to.
 * @param[in] our_addr Our BLE address for this connection.
 */
void lowpan6_ble_netif_up(esp_netif_t* esp_netif, ble_addr_t* peer_addr, ble_addr_t* our_addr);

/** Bring down the netif upon disconnection.
 *
 * @param[in] esp_netif The netif to mark down.
 */
void lowpan6_ble_netif_down(esp_netif_t* netif);

/** Convert a NimBLE BLE addr to an EUI64 identifier.
 *
 * NimBLE stores its BLE addresses in the reverse order that LwIP's `ble_addr_to_eui64` function
 * expects. This is a quick wrapper to help flip those bytes before calling `ble_addr_to_eui64`.
 *
 * @param[in] addr The BLE addr to translate from.
 * @param[out] eui64 The resulting EUI64 address.
 */
void nimble_addr_to_eui64(ble_addr_t const* addr, uint8_t* eui64);

/** Create a link-local address from an EUI64 identifier.
 *
 * An EUI64 interface identifier can be formed from a 48-bit Bluetooth device address by inserting
 * the octets 0xFF and 0xFE in the middle of the Bluetooth device address.
 *
 * A link-local IPv6 address is then formed by prepending the EUI64 address with the prefix
 * FE80::/64.
 *
 * BLE address (48 bits):       00:11:22:33:44:55
 * EUI identifier (64 bits):    00:11:22:FF:FE:33:44:55
 * IPv6 link-local address:     FE80:0000:0000:0000:0011:22FF:FE33:4455
 *
 * Use our `nimble_addr_to_eui64` helper to form an EUI64 address from a `ble_addr_t`.
 *
 * @example
 * ```c
 * ble_addr_t addr;
 * uint8_t eui64_addr[8];
 * nimble_addr_to_eui64(&addr, eui64_addr);
 * ipv6_create_link_local_from_eui64(eui64_addr, dst);
 * ```
 *
 * @param[in] src The source EUI64 address. MUST contain at least 8 bytes.
 * @param[out] dst The resulting IPv6 address.
 */
void ipv6_create_link_local_from_eui64(uint8_t const* src, ip6_addr_t* dst);
