idf_component_register(SRCS "shiny_module_dce.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_modem)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
