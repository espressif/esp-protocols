/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <unistd.h>
#include "sdkconfig.h"

#ifdef CONFIG_IDF_TARGET_LINUX
// namespace with esp_ on linux to avoid conflict of symbols
#define gethostname esp_gethostname
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Retrieves the hostname of the device.
 *
 * This function provides the hostname associated with the network interface.
 * Unlike the standard behavior where the hostname represents a system-wide name,
 * this implementation returns lwip netif hostname (used as a hostname in DHCP packets)
 *
 * @param[out] name A pointer to a buffer where the hostname will be stored.
 *                  The buffer must be allocated by the caller.
 * @param[in] len   The size of the buffer pointed to by @p name. The hostname,
 *                  including the null-terminator, must fit within this size.
 *
 * @return
 *   - 0 on success
 *   - -1 on error, with `errno` set to indicate the error:
 *     - `EINVAL`: Invalid argument, name is NULL, or hostname is too long
 *
 * @note This implementation retrieves the hostname associated with the network
 * interface using the `esp_netif_get_hostname()` function, which in turn
 * returns lwip netif hostname used in DHCP packets if LWIP_NETIF_HOSTNAME=1 (hardcoded)
 * in ESP-IDF lwip port.
 * As there could be multiple network interfaces in the system, the logic tries
 * to find the default (active) netif first, then it looks for any (inactive) netif
 * with highest route priority. If none of the above found or esp_netif_get_hostname() fails
 * for the selected interface, this API returns the default value of `CONFIG_LWIP_LOCAL_HOSTNAME`,
 * the local hostname from lwip component configuration menu.
 */
int gethostname(char *name, size_t len);

#ifdef __cplusplus
}
#endif
