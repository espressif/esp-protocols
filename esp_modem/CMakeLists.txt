set(srcs "src/esp_modem_dte.cpp"
        "src/esp_modem_netif.cpp"
        "src/esp_modem_api.cpp"
        "src/esp_modem_factory.cpp"
        "src/esp_modem_cmux.cpp"
        "src/esp_modem_command_library.cpp"
        "src/esp_modem_modules.cpp"
        "src/esp_modem_uart.cpp")

set(include_dirs "include")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    PRIV_INCLUDE_DIRS private_include
                    REQUIRES driver)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)