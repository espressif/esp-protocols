idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    set(platform_srcs src/esp_modem_primitives_linux.cpp
        src/esp_modem_uart_linux.cpp
        src/esp_modem_netif_linux.cpp)
    set(dependencies esp_system_protocols_linux)
else()
    set(platform_srcs src/esp_modem_primitives_freertos.cpp
                      src/esp_modem_uart.cpp
                      src/esp_modem_term_uart.cpp
                      src/esp_modem_netif.cpp)
    set(dependencies driver)
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


# If CMake doesn't know C++17 features, set it manually via comile options
# esp-modem component requires C++17 internally, but older CMake (< 3.8, but still supported in IDF)
# doesn't support target_compile_features() for cxx_std_17.
get_property(cxx_known_features GLOBAL PROPERTY CMAKE_CXX_KNOWN_FEATURES)
if ("cxx_std_17" IN_LIST cxx_known_features)
    target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
else()
    target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
endif()

if(${target} STREQUAL "linux")
    # This is needed for ESP_LOGx() macros, as integer formats differ on ESP32(..) and x64
    set_target_properties(${COMPONENT_LIB} PROPERTIES COMPILE_FLAGS -Wno-format)
endif()
