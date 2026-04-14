cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS
    ../..                                                   # esp_modem component
    ../../port/linux                                        # esp_event_mock, esp_system_protocols_linux
    ../../../../common_components/linux_compat/esp_timer    # esp_timer for linux
    ../../../../common_components/linux_compat/freertos     # freertos shim for linux
)

set(COMPONENTS main)
project(host_test_app)
