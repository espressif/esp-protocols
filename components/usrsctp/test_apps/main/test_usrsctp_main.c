/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Embedded smoke test for usrsctp. Validates that the component
 * links cleanly on the chosen target and that usrsctp_init/finish work.
 *
 * Deeper coverage (real SCTP association over the userspace stack)
 * lives in host_test/ under the IDF Linux target.
 */
#include <stdio.h>
#include "unity.h"
#include "usrsctp.h"

/* Empty stub used as the "outgoing packet" callback during the smoke
 * init; we never actually send anything in this test. */
static int dummy_conn_output(void *addr, void *buf, size_t length,
                             uint8_t tos, uint8_t set_df)
{
    (void)addr; (void)buf; (void)length; (void)tos; (void)set_df;
    return 0;
}

TEST_CASE("usrsctp_init then usrsctp_finish succeed", "[usrsctp]")
{
    usrsctp_init(0, dummy_conn_output, NULL);
    /* usrsctp_finish() returns the number of remaining endpoints; 0 = clean. */
    TEST_ASSERT_EQUAL(0, usrsctp_finish());
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
