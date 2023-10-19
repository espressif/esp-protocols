# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

# -*- coding: utf-8 -*-
import pytest


@pytest.mark.esp32
def test_examples_ifconfig_command(dut):
    dut.expect('esp>', timeout=30)
    dut.write('ifconfig eth init')
    dut.expect(r'Internal\(IP101\): pins:', timeout=30)
    dut.write('ifconfig netif create 0')
    dut.expect(r'ethernet_init: Ethernet\(IP101\[23,18\]\) Link Up', timeout=30)
    dut.write('ifconfig')
    dut.expect('console_ifconfig: Interface Name: en1', timeout=5)
    dut.write('ifconfig netif destroy en1')
    dut.expect('esp>', timeout=5)
    dut.write('ifconfig eth deinit')
    dut.expect('esp>', timeout=5)
    pass
