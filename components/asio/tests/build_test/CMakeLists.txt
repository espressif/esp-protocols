# Verifies that a non-main component can consume asio's public headers
# (which transitively include "socketpair.h" from sock_utils) by only
# declaring `REQUIRES asio`.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(asio_build_test)
