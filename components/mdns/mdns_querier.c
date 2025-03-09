/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mdns_private.h"
#include "mdns_querier.h"
#include "mdns_mem_caps.h"

void _mdns_query_results_free(mdns_result_t *results)
{
    mdns_result_t *r;
    mdns_ip_addr_t *a;

    while (results) {
        r = results;

        mdns_mem_free((char *)(r->hostname));
        mdns_mem_free((char *)(r->instance_name));
        mdns_mem_free((char *)(r->service_type));
        mdns_mem_free((char *)(r->proto));

        for (size_t i = 0; i < r->txt_count; i++) {
            mdns_mem_free((char *)(r->txt[i].key));
            mdns_mem_free((char *)(r->txt[i].value));
        }
        mdns_mem_free(r->txt);
        mdns_mem_free(r->txt_value_len);

        while (r->addr) {
            a = r->addr;
            r->addr = r->addr->next;
            mdns_mem_free(a);
        }

        results = results->next;
        mdns_mem_free(r);
    }
}
