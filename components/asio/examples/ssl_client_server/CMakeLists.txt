# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXCLUDE_COMPONENTS openssl)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(asio_ssl_client_server)
