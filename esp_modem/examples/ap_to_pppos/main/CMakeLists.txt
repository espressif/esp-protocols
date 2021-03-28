idf_component_register(SRCS "ap2pppos_example_main.c"
                            "NetworkDCE.cpp"
                    INCLUDE_DIRS ".")

#target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)