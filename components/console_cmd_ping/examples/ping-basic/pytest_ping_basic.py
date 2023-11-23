# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

# -*- coding: utf-8 -*-
import pytest


@pytest.mark.esp32
def test_examples_ping_command(dut):
    dut.expect('esp>', timeout=30)
    dut.write('ping 127.0.0.1')
    dut.expect('5 packets transmitted, 5 received, 0% packet loss, time 0ms', timeout=30)
    pass
