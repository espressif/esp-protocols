# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0


def pytest_addoption(parser):
    parser.addoption(
        "--uri",
        action="store",
        default=None,
        help="Server URI to send via serial (stdin)",
    )
