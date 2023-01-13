idf_component_register(SRCS "pppd_test.cpp"
                            "NetworkDCE.cpp"
        INCLUDE_DIRS "$ENV{IDF_PATH}/tools/catch"
        REQUIRES esp_modem)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
