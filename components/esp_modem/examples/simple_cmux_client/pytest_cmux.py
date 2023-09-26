# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
from __future__ import print_function, unicode_literals


def test_cmux_connection(dut):
    """
    steps:
      1. initializes connection with SIM800
      2. checks we get an IP
      3. checks for the MQTT events
    """
    # Check the sequence of connecting, publishing, disconnecting
    dut.expect('Modem has correctly entered multiplexed')
    # Check for MQTT connection and the data event
    dut.expect('TOPIC: /topic/esp-modem')
    dut.expect('DATA: Hello modem')
