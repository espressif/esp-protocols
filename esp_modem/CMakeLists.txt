idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    set(platform_srcs src/esp_modem_primitives_linux.cpp
        src/esp_modem_uart_linux.cpp
        src/esp_modem_netif_linux.cpp)
    set(dependencies esp_system_protocols_linux)
else()
    set(platform_srcs src/esp_modem_primitives_freertos.cpp
                      src/esp_modem_uart.cpp
                      src/esp_modem_netif.cpp)
    set(dependencies driver)
endif()

set(srcs ${platform_srcs}
        "src/esp_modem_dte.cpp"
        "src/esp_modem_dce.cpp"
        "src/esp_modem_api.cpp"
        "src/esp_modem_factory.cpp"
        "src/esp_modem_cmux.cpp"
        "src/esp_modem_command_library.cpp"
        "src/esp_modem_modules.cpp")

set(include_dirs "include")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    PRIV_INCLUDE_DIRS private_include
                    REQUIRES ${dependencies})

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
