# SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
from __future__ import print_function, unicode_literals


def test_cmux_connection(dut):
    """
    steps:
      1. checks we're in CMUX mode and get an IP
      2. checks for ping command
    """
    # Check we're in CMUX mode and get an IP
    dut.expect('Modem has correctly entered multiplexed command/data mode', timeout=60)
    # Check for ping command
    dut.expect('Ping command finished with return value: 0', timeout=30)
