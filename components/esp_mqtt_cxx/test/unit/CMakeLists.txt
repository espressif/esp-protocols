cmake_minimum_required(VERSION 3.5)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
if(${IDF_TARGET} STREQUAL "linux")
    set(EXTRA_COMPONENT_DIRS "../../../../common_components/linux_compat")
    set(COMPONENTS main)
endif()

project(esp_mqtt_cxx_host_test)
