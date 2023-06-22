# This project serves as a demo to enable using esp-mqtt on ESP platform targets as well as on linux
cmake_minimum_required(VERSION 3.16)

# For ESP32 platform target
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

if("${IDF_TARGET}" STREQUAL "linux")
# For linux-target we have two options:
# - With lwIP (must be defined on command line, e.g. idf.py -DWITH_LWIP=1)
#     access networking from linux `tap` interface (TAP networking mode)
# - Without lwIP (must be defined on command line, e.g. idf.py -DWITH_LWIP=0)
#     no designated interface, accesses user network via linux/socket sys calls
    if(WITH_LWIP STREQUAL 1)
        set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_tapif_io
                                 "../../common_components/linux_compat/esp_timer")
        set(COMPONENTS main esp_netif lwip protocol_examples_tapif_io startup esp_hw_support esp_system nvs_flash mqtt esp_timer)
    else()
        list(APPEND EXTRA_COMPONENT_DIRS
                "../../common_components/linux_compat/esp_timer"
                "$ENV{IDF_PATH}/examples/protocols/linux_stubs/esp_stubs")
        set(COMPONENTS main nvs_flash esp-tls esp_stubs mqtt protocol_examples_common esp_timer)
    endif()
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_mqtt_demo)
