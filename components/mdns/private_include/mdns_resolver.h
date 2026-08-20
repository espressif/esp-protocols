/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include "mdns_private.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Free all resolvers.
 */
void mdns_priv_resolver_free(void);

/**
 * @brief Perform action from mdns service queue
 *
 * @note Called by `free_action()` and `execute_action()` in mdns_service.c
 *
 * @param action Pointer to the action to perform.
 * @param type Type of the action.
 */
void mdns_priv_resolver_action(mdns_action_t *action, mdns_action_subtype_t type);

#ifdef __cplusplus
}
#endif
