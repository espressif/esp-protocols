from __future__ import unicode_literals

import pytest
from pytest_embedded import Dut


def test_examples_asio_ssl(dut):
    dut.expect('Reply: GET / HTTP/1.1')
