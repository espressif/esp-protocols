set(srcs "src/esp_modem.c"
        "src/esp_modem_dce_command_lib.c"
        "src/esp_modem_dce_common_commands.c"
        "src/esp_modem_dce.c"
        "src/esp_modem_dte.c"
        "src/esp_modem_netif.c"
        "src/esp_modem_recov_helper.c"
        "src/esp_sim800.c"
        "src/esp_sim7600.c"
        "src/esp_bg96.c"
        "src/esp_modem_dte.cpp"
        "src/ppp_netif.cpp"
        "src/esp_modem_commands.cpp"
        "src/esp_modem_api.cpp"
        "src/esp_modem_dce.cpp"
        "src/esp_modem_device.cpp"
        "src/esp_modem_device_factory.cpp"
        "src/esp_modem_uart.cpp")

set(include_dirs "include")

if(CONFIG_MODEM_LEGACY_API)
        list(APPEND srcs "src/esp_modem_compat.c")
        list(APPEND include_dirs "include_compat")
endif()

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    PRIV_INCLUDE_DIRS private_include
                    REQUIRES driver)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)