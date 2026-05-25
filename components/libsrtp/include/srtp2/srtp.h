/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Thin shim that re-exposes upstream cisco/libsrtp's public API header
 * under the `srtp2/` install-path namespace, so downstream consumers
 * can use the canonical `#include <srtp2/srtp.h>` form without us
 * vendoring a copy of the upstream header in this repo.
 */
#include <srtp.h>
