# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
idf_component_register(
    SRCS "host_test_main.c"
    INCLUDE_DIRS "."
    REQUIRES usrsctp
)
