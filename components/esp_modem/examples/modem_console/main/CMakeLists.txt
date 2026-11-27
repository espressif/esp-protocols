if(CONFIG_ESP_MODEM_ENABLE_DEVELOPMENT_MODE)
    set(command_dir "generate")
else()
    set(command_dir "command")
endif()

set(srcs "modem_console_main.cpp"
         "console_helper.cpp"
         "httpget_handle.c"
         "ping_handle.c")

if(CONFIG_EXAMPLE_MODEM_DEVICE_NETWORK)
    list(APPEND srcs "NetworkDCE.cpp")
else()
    list(APPEND srcs "${command_dir}/my_module_dce.cpp")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "." "${command_dir}")
