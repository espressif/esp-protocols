# Build-only regression test for https://github.com/espressif/esp-protocols/issues/... (PR 2403)
# Verifies that a non-main component can consume asio's public headers
# (which transitively include "socketpair.h" from sock_utils) by only
# declaring `REQUIRES asio`.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS ../..)

project(asio_build_test)
