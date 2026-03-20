cmake_minimum_required(VERSION 3.16)

set(COMPONENTS main)
set(SOCK_UTILS_HOST_TESTS 1)    # Force linux build of sock_utils component
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(sock_utils_host_test)
