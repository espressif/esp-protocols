# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

# -*- coding: utf-8 -*-

import pytest


@pytest.mark.esp32
def test_esp_dns_resolution(dut):
    """
    Test DNS resolution for different protocols (UDP, TCP, DoT, DoH).
    """
    dut.expect('Executing UDP DNS', timeout=10)
    dut.expect('Executing TCP DNS', timeout=10)
    dut.expect('Executing DNS over TLS', timeout=10)
    dut.expect('Executing DNS over HTTPS', timeout=10)

    # Check for successful DNS resolution
    dut.expect('Hostname: yahoo.com:', timeout=10)
    dut.expect('Hostname: www.google.com:', timeout=10)
    dut.expect('Hostname: 0.0.0.0:', timeout=10)
    dut.expect('Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100', timeout=10)

    # Check for system information logs
    dut.expect('Free Heap:', timeout=10)
    dut.expect('Min Free Heap:', timeout=10)
    dut.expect('Stack High Water Mark:', timeout=10)
