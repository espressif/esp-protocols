# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

# -*- coding: utf-8 -*-

import pytest


@pytest.mark.esp32
def test_examples_getaddrinfo(dut):
    dut.expect('esp>', timeout=30)
    dut.write('getaddrinfo www.google.com')
    dut.expect(r'getaddrinfo', timeout=30)
    dut.expect(r'getaddrinfo', timeout=30)
    dut.expect(r'getaddrinfo', timeout=30)
    dut.expect(r'getaddrinfo', timeout=30)
    dut.expect(r'getaddrinfo', timeout=30)
