/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Embedded smoke test for libsrtp. Validates that the component
 * links cleanly on the chosen target and exposes the libsrtp public
 * API. Deeper coverage lives in host_test/ (runs against the IDF
 * Linux target and reuses upstream libsrtp's own test suite).
 */
#include <string.h>
#include "unity.h"
#include "srtp2/srtp.h"

TEST_CASE("srtp_init then srtp_shutdown succeed", "[srtp2]")
{
    TEST_ASSERT_EQUAL(srtp_err_status_ok, srtp_init());
    TEST_ASSERT_EQUAL(srtp_err_status_ok, srtp_shutdown());
}

TEST_CASE("srtp_get_version_string returns non-empty", "[srtp2]")
{
    const char *v = srtp_get_version_string();
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_GREATER_THAN(0, strlen(v));
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
