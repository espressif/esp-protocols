# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import glob
from pathlib import Path

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.host_test
@pytest.mark.skipif(
    not bool(glob.glob(f'{Path(__file__).parent.absolute()}/build*/')),
    reason='Skip the idf version that not build',
)
@idf_parametrize('target', ['linux'], indirect=['target'])
def test_libsrtp_linux(dut: Dut) -> None:
    dut.expect_exact('libsrtp host_test: PASS', timeout=30)
