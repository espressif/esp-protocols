# SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

# -*- coding: utf-8 -*-
import pytest


@pytest.mark.esp32
def test_examples_user_command(dut):
    # Phase 1 runs "user" before the REPL starts, then stops the console.
    dut.expect('Hello from user command', timeout=30)
    # Phase 2 re-inits with the custom prompt and registers the extra commands.
    dut.expect('Plugin count: 1', timeout=30)
    dut.expect('cfg>', timeout=30)
    dut.write('user')
    dut.expect('Hello from user command', timeout=30)
    dut.write('echo hello')
    dut.expect('hello', timeout=30)
    dut.write('plug')
    dut.expect('Hello from user command', timeout=30)
    dut.write('help')
    dut.expect('Print a greeting', timeout=30)
    dut.expect('Print an argument', timeout=30)
    dut.expect('Registered by the example plugin', timeout=30)
