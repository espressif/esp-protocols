# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if("${IDF_TARGET}" STREQUAL "linux")
    list(APPEND EXTRA_COMPONENT_DIRS "../../../../common_components/linux_compat")
endif()

idf_build_set_property(MINIMAL_BUILD ON)

project(broker)
