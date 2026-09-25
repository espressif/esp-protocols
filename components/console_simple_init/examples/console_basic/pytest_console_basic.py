# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

# -*- coding: utf-8 -*-
import pytest


@pytest.mark.esp32
def test_examples_user_command(dut):
    dut.expect('esp>', timeout=30)
    dut.write('user')
    dut.expect('Hello from user command', timeout=30)
    dut.write('help')
    dut.expect('User defined command', timeout=30)
    dut.write('not_a_command')
    dut.expect('Unrecognized command', timeout=30)
