# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "..")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_app_usrsctp)
