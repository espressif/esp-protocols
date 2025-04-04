if(CONFIG_ESP_MODEM_ENABLE_DEVELOPMENT_MODE)
    set(command_dir "generate")
else()
    set(command_dir "command")
endif()

idf_component_register(SRCS "SIM7070_gnss.cpp"
                    INCLUDE_DIRS "." "${command_dir}"
                    PRIV_REQUIRES esp_modem)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
