/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

struct ppp_info_t {
    iface_info_t parent;
    void *context;
    bool stop_task;
};

extern const esp_netif_driver_ifconfig_t *ppp_driver_cfg;

void ppp_task(void *args);

void ppp_destroy_context(struct ppp_info_t *ppp_info);
