# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/version.cmake)

# Add newly added components to one of these lines:
set(EXTRA_COMPONENT_DIRS
    ../components/eppp_link
    ../components/esp_modem
    ../components/esp_mqtt_cxx
    ../components/esp_websocket_client
    ../components/console_cmd_ifconfig
    ../components/console_cmd_ping
    ../components/console_cmd_wifi
    ../components/console_simple_init
    ../components/mbedtls_cxx
    ../components/mdns)


# !This section should NOT be touched when adding new component!
# Take all components in EXTRA_COMPONENT_DIRS, strip leading '../' and add it to TEST_COMPONENTS
# The build system will build and link unit tests, if the component contains 'test' subdirectory
set(TEST_COMPONENTS "" CACHE STRING "List of components to test")
foreach (CMP_DIR ${EXTRA_COMPONENT_DIRS})
    string(SUBSTRING ${CMP_DIR} 3 100 STRIPPED_CMP) # There should be no component name longer than 100 bytes...
    list(APPEND TEST_COMPONENTS ${STRIPPED_CMP})
endforeach()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_protocols_test_app)
