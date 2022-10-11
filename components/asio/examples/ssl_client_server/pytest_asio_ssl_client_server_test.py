# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
from __future__ import unicode_literals


def test_examples_asio_ssl(dut):
    dut.expect('Reply: GET / HTTP/1.1')
