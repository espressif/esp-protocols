# The following four lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# (Not part of the boilerplate)
# This example uses an extra component for common functions such as Wi-Fi and Ethernet connection.
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

if(${IDF_TARGET} STREQUAL "linux")
    list(APPEND EXTRA_COMPONENT_DIRS "../../common_components/linux_compat/freertos"
            "$ENV{IDF_PATH}/examples/protocols/linux_stubs/esp_stubs")
    set(COMPONENTS main nvs_flash)
endif()


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mqtt_tcp)
