# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

# -*- coding: utf-8 -*-

import pytest


@pytest.mark.esp32
def test_examples_ifconfig_command(dut):
    dut.expect('esp>', timeout=30)
    dut.write('wifi show network')
    dut.expect(r'Showing Wifi networks', timeout=30)
    dut.expect('esp>', timeout=30)
