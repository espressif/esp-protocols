cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

list(APPEND EXTRA_COMPONENT_DIRS "../../../../common_components/linux_compat")

idf_build_set_property(MINIMAL_BUILD ON)

project(mosquitto_host_test)
