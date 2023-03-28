idf_component_register(SRCS "modem_console_main.cpp"
                            "console_helper.cpp"
                            "httpget_handle.c"
                            "ping_handle.c"
                       REQUIRES console esp_http_client nvs_flash
                       INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
