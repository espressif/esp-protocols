# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

# -*- coding: utf-8 -*-

import re


def test_examples_asio_chat(dut):
    msg = 'asio-chat: received hi'
    # start the test and expect the client to receive back it's original data
    dut.expect(re.compile(b'Waiting for input'), timeout=30)
    dut.write(msg)
    dut.write('exit')
    msg = bytes(msg, encoding='utf8')
    dut.expect(re.compile(msg), timeout=30)
