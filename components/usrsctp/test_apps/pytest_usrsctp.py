# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import pytest


@pytest.mark.generic
@pytest.mark.parametrize("target", ["esp32", "esp32c3"], indirect=True)
def test_usrsctp_smoke(dut):
    dut.expect_unity_test_output(timeout=30)
