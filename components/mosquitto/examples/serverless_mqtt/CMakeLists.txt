# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Setup ESP-PEER from GitHub repo (but it's supported only on certain targets)
set(ESP_PEER_COMPATIBLE_TARGETS "esp32s2" "esp32s3" "esp32p4" "esp32")
if(IDF_TARGET IN_LIST ESP_PEER_COMPATIBLE_TARGETS)
execute_process(COMMAND ${CMAKE_BINARY_DIR}/../esp_peer_setup/install.sh
                ${CMAKE_BINARY_DIR}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                RESULT_VARIABLE script_result)

if(script_result)
    message(FATAL_ERROR "Script esp_peer_setup.sh failed with exit code ${script_result}")
endif()
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_BINARY_DIR}/esp-peer/components/")
else()
    message(STATUS "ESP-PEER is not compatible with this target")
endif()

project(serverless_mqtt)
