idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    set(platform_srcs src/esp_modem_primitives_linux.cpp
        src/esp_modem_uart_linux.cpp
        src/esp_modem_netif_linux.cpp)
    set(dependencies esp_system_protocols_linux)
else()
    set(platform_srcs src/esp_modem_primitives_freertos.cpp
                      src/esp_modem_api_target.cpp
                      src/esp_modem_uart.cpp
                      src/esp_modem_term_uart.cpp
                      src/esp_modem_netif.cpp)
    set(dependencies driver esp_event esp_netif)
endif()


set(srcs ${platform_srcs}
        "src/esp_modem_dte.cpp"
        "src/esp_modem_dce.cpp"
        "src/esp_modem_api.cpp"
        "src/esp_modem_c_api.cpp"
        "src/esp_modem_factory.cpp"
        "src/esp_modem_cmux.cpp"
        "src/esp_modem_command_library.cpp"
        "src/esp_modem_term_fs.cpp"
        "src/esp_modem_vfs_uart_creator.cpp"
        "src/esp_modem_vfs_socket_creator.cpp"
        "src/esp_modem_modules.cpp")

set(include_dirs "include")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    PRIV_INCLUDE_DIRS private_include
                    REQUIRES ${dependencies})


set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)


if(${target} STREQUAL "linux")
    # This is needed for ESP_LOGx() macros, as integer formats differ on ESP32(..) and x64
    set_target_properties(${COMPONENT_LIB} PROPERTIES COMPILE_FLAGS -Wno-format)
endif()

if(CONFIG_GCOV_ENABLED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")


    MARK_AS_ADVANCED(
        CMAKE_CXX_FLAGS_COVERAGE
        CMAKE_C_FLAGS_COVERAGE
        CMAKE_EXE_LINKER_FLAGS_COVERAGE
        CMAKE_SHARED_LINKER_FLAGS_COVERAGE )
endif()
