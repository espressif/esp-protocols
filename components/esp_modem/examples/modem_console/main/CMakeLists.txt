if(CONFIG_ESP_MODEM_ENABLE_DEVELOPMENT_MODE)
    set(command_dir "generate")
else()
    set(command_dir "command")
endif()

idf_component_register(SRCS "modem_console_main.cpp"
                            "console_helper.cpp"
                            "${command_dir}/my_module_dce.cpp"
                            "httpget_handle.c"
                            "ping_handle.c"
                       REQUIRES console esp_http_client nvs_flash
                       INCLUDE_DIRS "." "${command_dir}")
