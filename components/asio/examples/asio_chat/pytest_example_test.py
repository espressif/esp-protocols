# This example code is in the Public Domain (or CC0 licensed, at your option.)

# Unless required by applicable law or agreed to in writing, this
# software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.

# -*- coding: utf-8 -*-

import re
import pytest
from pytest_embedded import Dut


def test_examples_asio_chat(dut):
    msg = 'asio-chat: received hi'
    # start the test and expect the client to receive back it's original data
    dut.expect(re.compile(b'Waiting for input'), timeout=30)
    dut.write(msg)
    dut.write('exit')
    msg = bytes(msg, encoding='utf8')
    dut.expect(re.compile(msg), timeout=30)
