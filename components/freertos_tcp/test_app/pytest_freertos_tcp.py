# SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest


@pytest.mark.esp32
@pytest.mark.esp32c3
@pytest.mark.esp32s3
@pytest.mark.wifi_router
def test_freertos_tcp_basic(dut):
    """
    Test basic FreeRTOS-Plus-TCP functionality:
    - TCP connection
    - UDP send/receive
    - DNS resolution
    - DHCP acquisition
    """
    # Wait for WiFi connection
    dut.expect('WiFi connected', timeout=30)
    dut.expect('Got IP', timeout=30)

    # Check for test execution
    dut.expect('Running FreeRTOS-Plus-TCP tests', timeout=10)

    # Run Unity tests
    dut.expect_unity_test_output(timeout=120)
