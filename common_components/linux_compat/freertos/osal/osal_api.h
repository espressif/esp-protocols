/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// queue api
void *osal_queue_create(void);
void osal_queue_delete(void *q);
bool osal_queue_send(void *q, uint8_t *data, size_t len);
bool osal_queue_recv(void *q, uint8_t *data, size_t len, uint32_t ms);

// mutex api
void *osal_mutex_create(void);
void osal_mutex_delete(void *m);
void osal_mutex_take(void *m);
void osal_mutex_give(void *m);

// event groups
void *osal_signal_create(void);
void osal_signal_delete(void *s);
uint32_t osal_signal_clear(void *s, uint32_t bits);
uint32_t osal_signal_set(void *s, uint32_t bits);
uint32_t osal_signal_get(void *s);
uint32_t osal_signal_wait(void *s, uint32_t flags, bool all, uint32_t timeout);

#ifdef __cplusplus
}
#endif
