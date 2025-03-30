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

# Add option to enable unit testing
option(ENABLE_UNIT_TESTS "Enable Unity-based unit tests" OFF)

# Add preprocessor definition based on the option
if(ENABLE_UNIT_TESTS)
    add_definitions(-DENABLE_UNIT_TESTS)
endif()

# Add CMock options
if(ENABLE_UNIT_TESTS)
    # CMock paths
    set(CMOCK_DIR "$ENV{IDF_PATH}/components/cmock")

    # Use ruby command directly instead of looking for ruby.exe
    find_program(RUBY_EXECUTABLE ruby)
    if(NOT RUBY_EXECUTABLE)
        message(FATAL_ERROR "Ruby is required for CMock but was not found!")
    endif()

    # Define list of files to mock
    set(MOCK_FILES
        "mdns_pcb"
        "mdns_send"
        # Add more files here as needed, without .h extension
    )

    # Verify headers exist and create mock commands for each
    foreach(mock_file ${MOCK_FILES})
        set(header_path "${COMPONENT_DIR}/private_include/${mock_file}.h")
        if(NOT EXISTS ${header_path})
            message(FATAL_ERROR "Cannot find ${mock_file}.h at ${header_path}")
        endif()

        list(APPEND MOCK_OUTPUTS
            ${CMAKE_CURRENT_BINARY_DIR}/mocks/mock_${mock_file}.c
            ${CMAKE_CURRENT_BINARY_DIR}/mocks/mock_${mock_file}.h
        )

        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mocks/mock_${mock_file}.c
                   ${CMAKE_CURRENT_BINARY_DIR}/mocks/mock_${mock_file}.h
            COMMAND ${RUBY_EXECUTABLE}
                    ${CMOCK_DIR}/CMock/lib/cmock.rb
                    -o${CMAKE_CURRENT_SOURCE_DIR}/cmock_config.yml
                    ${header_path}
            DEPENDS ${header_path}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating mock for ${mock_file}.h"
        )
    endforeach()

    # Include CMock headers
    include_directories(
        ${CMOCK_DIR}/CMock/src
        ${CMAKE_CURRENT_BINARY_DIR}/mocks
        ${UNITY_DIR}/unity/src
        ${UNITY_DIR}/include
    )

    # Create directory for generated mocks
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mocks)

    # Create CMock config file if it doesn't exist
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmock_config.yml)
        file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/cmock_config.yml
"---
:cmock:
  :mock_prefix: mock_
  :plugins:
    - :callback
    - :ignore
    - :expect_any_args
    - :array
    - :return_thru_ptr
  :strippables:
    - '(?:extern|static)\\s+'
  :treat_as:
    uint8_t: HEX8
    uint16_t: HEX16
    uint32_t: UINT32
    int8_t: INT8
    bool: UINT8"
        )
    endif()
endif()

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
)

if(ENABLE_UNIT_TESTS)
    # Remove real implementations of mocked files
    foreach(mock_file ${MOCK_FILES})
        list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../../${mock_file}.c)
    endforeach()

    # Add test-related sources
    list(APPEND SOURCES
        ${UNITY_DIR}/unity/src/unity.c
        ${CMOCK_DIR}/CMock/src/cmock.c
    )

    # Add all generated mock files
    foreach(mock_file ${MOCK_FILES})
        list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/mocks/mock_${mock_file}.c)
    endforeach()
endif()

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

# Enable testing if unit tests are enabled
if(ENABLE_UNIT_TESTS)
    enable_testing()
    add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME} --test)
endif()

# Add libbsd dependency
#find_package(PkgConfig REQUIRED)
#pkg_check_modules(LIBBSD REQUIRED libbsd-overlay)

# Link against libbsd for strlcat
#target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBBSD_LIBRARIES})
#target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fsanitize=undefined)
#target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fsanitize=undefined)

# Add include directories if needed
#target_include_directories(${PROJECT_NAME} PRIVATE ${LIBBSD_INCLUDE_DIRS})

# Add mock generation as dependency
if(ENABLE_UNIT_TESTS)
    add_custom_target(generate_mocks
        DEPENDS ${MOCK_OUTPUTS}
    )
    add_dependencies(${PROJECT_NAME} generate_mocks)
endif()
