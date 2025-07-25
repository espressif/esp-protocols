set(module_dir "generic_module")
if (CONFIG_EXAMPLE_MODEM_DEVICE_BG96)
    set(device_srcs sock_commands_bg96.cpp)
elseif(CONFIG_EXAMPLE_MODEM_DEVICE_SIM7600)
    set(device_srcs sock_commands_sim7600.cpp)
elseif(CONFIG_EXAMPLE_MODEM_DEVICE_ESPAT)
    set(device_srcs sock_commands_espat.cpp)
    set(module_dir "espat_module")
endif()

if(CONFIG_ESP_MODEM_ENABLE_DEVELOPMENT_MODE)
    set(command_dir "generate")
else()
    set(command_dir "command")
endif()

idf_component_register(SRCS "modem_client.cpp"
                            "${command_dir}/sock_dce.cpp"
                            "tcp_transport_at.cpp"
                            "${device_srcs}"
                       INCLUDE_DIRS "." "${command_dir}" "${module_dir}")
