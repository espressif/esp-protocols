cmake_minimum_required(VERSION 3.5)
if(ESP_PLATFORM)
    # Used for reconfiguration of the project
    # Executed by idf.py reconfigure
    set(EXTRA_COMPONENT_DIRS ../../)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(mdns_host_unit_test_config)
    return()
endif()

project(mdns_host_unit_test C)

# Unit tests are enabled only for a real suite name (fuzz builds leave this OFF).
set(UNIT_TESTS "OFF" CACHE STRING "Unit tests: OFF, test_receiver, test_sender, test_browse")
set_property(CACHE UNIT_TESTS PROPERTY STRINGS OFF test_receiver test_sender test_browse)

if(UNIT_TESTS AND NOT UNIT_TESTS STREQUAL "OFF")
    set(ENABLE_UNIT_TESTS 1)
    message(STATUS "Unit testing enabled with UNIT_TESTS=${UNIT_TESTS}")
else()
    unset(ENABLE_UNIT_TESTS)
    message(STATUS "Unit testing disabled (fuzz/harness build)")
endif()

# Set variables for directories
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(STUB_DIR ${TEST_DIR}/stubs)
set(MDNS_DIR ${TEST_DIR}/../../)
set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../)
set(ESP_NETIF_LINUX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../host_test/components/esp_netif_linux/include)
set(COMMON_COMPONENTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../common_components/linux_compat/)
set(IDF_COMPONENTS_DIR "$ENV{IDF_PATH}/components")

include_directories(
    ${TEST_DIR}
    ${STUB_DIR}
    ${TEST_DIR}/build/config
    ${COMMON_COMPONENTS_DIR}/freertos/include
    ${COMMON_COMPONENTS_DIR}/esp_timer/include
    ${IDF_COMPONENTS_DIR}/esp_event/include
    ${IDF_COMPONENTS_DIR}/esp_netif/include
    ${IDF_COMPONENTS_DIR}/esp_common/include
    ${IDF_COMPONENTS_DIR}/esp_system/include
    ${IDF_COMPONENTS_DIR}/log/include
    ${IDF_COMPONENTS_DIR}/esp_hw_support/etm/include
    ${IDF_COMPONENTS_DIR}/soc/linux/include
    ${IDF_COMPONENTS_DIR}/esp_rom/include
    ${IDF_COMPONENTS_DIR}/heap/include
    ${IDF_COMPONENTS_DIR}/esp_rom/linux/include/linux/
    ${ESP_NETIF_LINUX_DIR}
    ${COMPONENT_DIR}
    ${COMPONENT_DIR}/include
    ${COMPONENT_DIR}/private_include
)

# Source files
set(SOURCES
    ${STUB_DIR}/mdns_mem_caps.c
    ${STUB_DIR}/esp_idf.c
    ${STUB_DIR}/mdns_networking.c
    ${STUB_DIR}/mdns_engine.c
    ${MDNS_DIR}/mdns_receive.c
    ${MDNS_DIR}/mdns_utils.c
    ${MDNS_DIR}/mdns_browser.c
    ${MDNS_DIR}/mdns_querier.c
    ${MDNS_DIR}/mdns_responder.c
    ${MDNS_DIR}/mdns_send.c
    ${MDNS_DIR}/mdns_pcb.c
    ${MDNS_DIR}/mdns_netif.c
    ${MDNS_DIR}/mdns_cache.c
)

if(ENABLE_UNIT_TESTS)
    include(unity/unit_test.cmake)
else()
    # Fuzz harness target: receive (parse/query) or browse (cache/TXT path)
    set(FUZZ_TARGET "receive" CACHE STRING "Fuzz harness: receive, browse")
    set_property(CACHE FUZZ_TARGET PROPERTY STRINGS receive browse)
    if(FUZZ_TARGET STREQUAL "browse")
        add_definitions(-DFUZZ_TARGET_BROWSE=1)
    elseif(FUZZ_TARGET STREQUAL "receive")
        add_definitions(-DFUZZ_TARGET_RECEIVE=1)
    else()
        message(FATAL_ERROR "FUZZ_TARGET must be 'receive' or 'browse' (got '${FUZZ_TARGET}')")
    endif()
    message(STATUS "Fuzz harness target: ${FUZZ_TARGET}")
    list(APPEND SOURCES main.c)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

# Add libbsd if available
find_library(LIB_BSD bsd)
if(LIB_BSD)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIB_BSD})
elseif(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message(WARNING "Missing LIBBSD library. Install libbsd-dev package and/or check linker directories.")
endif()

# Sanitizers for AFL fuzzing (unit tests enable these in unity/enable_testing.cmake)
if(NOT ENABLE_UNIT_TESTS)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -g3
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
endif()

# Enable testing if unit tests are enabled
if(ENABLE_UNIT_TESTS)
    include(unity/enable_testing.cmake)
endif()
