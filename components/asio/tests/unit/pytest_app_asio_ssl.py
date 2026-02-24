# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from pytest_embedded import Dut


def test_asio_ssl_unit(dut: Dut) -> None:
    dut.expect_unity_test_output()
