# The following four lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    set(test_component_dir $ENV{IDF_PATH}/tools/test_apps/components)
else()
    set(test_component_dir $ENV{IDF_PATH}/tools/unit-test-app/components)
endif()

set(EXTRA_COMPONENT_DIRS ${test_component_dir})

project(test_app)
