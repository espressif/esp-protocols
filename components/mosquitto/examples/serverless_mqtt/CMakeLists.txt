# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

execute_process(COMMAND ${CMAKE_BINARY_DIR}/../esp_peer_setup.sh
                ${CMAKE_BINARY_DIR}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                RESULT_VARIABLE script_result)

if(script_result)
    message(FATAL_ERROR "Script esp_peer_setup.sh failed with exit code ${script_result}")
endif()

list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_BINARY_DIR}/esp-peer/components/")

project(serverless_mqtt)
