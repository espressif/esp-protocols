idf_component_register(SRCS "pppd_test.cpp"
                            "NetworkDCE.cpp"
        INCLUDE_DIRS "$ENV{IDF_PATH}/tools/catch"
        REQUIRES esp_modem)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
