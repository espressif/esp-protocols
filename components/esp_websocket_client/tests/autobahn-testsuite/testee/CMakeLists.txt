cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Add Linux compatibility components if building for Linux target
# COMPONENTS must be set before include() to limit component scanning
if(${IDF_TARGET} STREQUAL "linux")
    set(COMPONENTS main)
endif()

# This override applies to this example only and does not touch ESP-IDF itself.
add_compile_options(-Wno-error=implicit-function-declaration)

project(autobahn_testee)
