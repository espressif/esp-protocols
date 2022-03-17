# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "../..")

include($ENV{IDF_PATH}/tools/cmake/version.cmake)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "4.4")
if(("${IDF_TARGET}" STREQUAL "esp32s2") OR ("${IDF_TARGET}" STREQUAL "esp32s3"))
    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/peripherals/usb/host/cdc/common")
endif()
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(modem-console)
