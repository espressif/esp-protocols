cmake_minimum_required(VERSION 3.5)
if(ESP_PLATFORM)
    set(EXTRA_COMPONENT_DIRS ../../)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(mdns_host_unit_test_config)
    return()
endif ()

project(mdns_host_unit_test C)

# Set variables for directories
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../)
set(ESP_NETIF_LINUX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../host_test/components/esp_netif_linux/include)
set(COMMON_COMPONENTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../common_components/linux_compat/)
set(IDF_COMPONENTS_DIR "$ENV{IDF_PATH}/components")

# Debug prints to see directory values
message(STATUS "TEST_DIR: ${TEST_DIR}")
message(STATUS "COMPONENT_DIR: ${COMPONENT_DIR}")

# Include directories for the test files and other required files
include_directories(
    ${TEST_DIR}
    ${TEST_DIR}/stubs
    ${TEST_DIR}/build/config
    ${COMMON_COMPONENTS_DIR}/freertos/include
    ${COMMON_COMPONENTS_DIR}/esp_timer/include
    ${IDF_COMPONENTS_DIR}/esp_event/include
    ${IDF_COMPONENTS_DIR}/esp_netif/include
    ${IDF_COMPONENTS_DIR}/esp_common/include
    ${IDF_COMPONENTS_DIR}/esp_system/include
    ${IDF_COMPONENTS_DIR}/log/include
    ${IDF_COMPONENTS_DIR}/esp_rom/include
    ${IDF_COMPONENTS_DIR}/heap/include
    ${IDF_COMPONENTS_DIR}/esp_rom/linux/include/linux/
    ${ESP_NETIF_LINUX_DIR}
    ${COMPONENT_DIR}
    ${COMPONENT_DIR}/include
    ${COMPONENT_DIR}/private_include
)

# Unity testing framework
set(UNITY_DIR "$ENV{IDF_PATH}/components/unity")
include_directories(${UNITY_DIR}/unity/src)

# Source files
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs/mdns_mem_caps.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs/esp_idf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs/mdns_networking.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs/mdns_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mdns_receive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mdns_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mdns_browser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mdns_querier.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mdns_responder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mdns_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mdns_pcb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mdns_netif.c
    ${UNITY_DIR}/unity/src/unity.c
)

# Enable sanitizers
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

# Setting C flags with debug symbols and disable optimization for better debugging
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")

# Create the test executable
add_executable(${PROJECT_NAME} ${SOURCES})

find_library(LIB_BSD bsd)
if(LIB_BSD)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIB_BSD})
elseif(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message(WARNING "Missing LIBBSD library. Install libbsd-dev package and/or check linker directories.")
endif()
# Setting C flags
#set_target_properties(${PROJECT_NAME} PROPERTIES
#    C_STANDARD 99
#    COMPILE_FLAGS "-Wall -Werror"
#)

# Test command with ASAN options
#enable_testing()
#add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
#set_tests_properties(${PROJECT_NAME} PROPERTIES
#    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1:verbosity=1:log_path=asan.log:print_stats=1:malloc_context_size=20"
#)

# Add libbsd dependency
#find_package(PkgConfig REQUIRED)
#pkg_check_modules(LIBBSD REQUIRED libbsd-overlay)

# Link against libbsd for strlcat
#target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBBSD_LIBRARIES})
#target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fsanitize=undefined)
#target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fsanitize=undefined)

# Add include directories if needed
#target_include_directories(${PROJECT_NAME} PRIVATE ${LIBBSD_INCLUDE_DIRS})
