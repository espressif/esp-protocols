cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if("${IDF_TARGET}" STREQUAL "linux")
    list(APPEND EXTRA_COMPONENT_DIRS "../../../../common_components/linux_compat")
endif()

idf_build_set_property(MINIMAL_BUILD ON)

project(websocket)
