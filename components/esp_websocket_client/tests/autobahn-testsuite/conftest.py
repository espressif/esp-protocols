# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0


def pytest_addoption(parser):
    parser.addoption(
        "--uri",
        action="store",
        default=None,
        help="Server URI to send via serial (stdin)",
    )
    parser.addoption(
        "--autobahn-timeout",
        action="store",
        type=int,
        default=2400,
        help="Overall test timeout in seconds; use 7200 for performance (9.*) runs",
    )
