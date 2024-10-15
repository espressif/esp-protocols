# SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

# -*- coding: utf-8 -*-

import pytest


@pytest.mark.esp32
def test_examples_ifconfig_command(dut):
    dut.expect('esp>', timeout=30)
    dut.write('help mqtt')
    dut.expect(r'mqtt  \[-CsD\] \[-h <host>\] \[-u <username>\] \[-P <password>\] \[--cert\] \[--key\] \[--cafile\]', timeout=30)

    dut.write('help mqtt_pub')
    dut.expect(r'mqtt_pub  \[-t <topic>\] \[-m <message>\]', timeout=30)

    dut.write('help mqtt_sub')
    dut.expect(r'mqtt_sub  \[-U\] \[-t <topic>\]', timeout=30)
