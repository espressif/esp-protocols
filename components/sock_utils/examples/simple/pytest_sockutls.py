# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

def test_sockutls(dut):
    """
    steps:
      1. Start the example
      2. Check the resultant IPv4 address from station netif
    """
    # Signal from the pipe simple implementation
    dut.expect('Received signal: IP4')
    # and the IPv4 address of the connected netif
    dut.expect('IPv4 address of interface "en1"')
