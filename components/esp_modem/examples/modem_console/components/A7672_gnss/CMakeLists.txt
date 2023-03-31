idf_component_register(
                    SRCS
                       "A7672_gnss.cpp"
                       "A7672_gnss_gnss_parser.cpp"
                       "A7672_gnss_gps_parser.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_modem)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
