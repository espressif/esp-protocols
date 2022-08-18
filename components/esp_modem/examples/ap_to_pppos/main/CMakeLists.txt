if(CONFIG_EXAMPLE_USE_MINIMAL_DCE)
    set(NETWORK_DCE "network_dce.cpp")
else()
    set(NETWORK_DCE "network_dce.c")
endif()

idf_component_register(SRCS "ap_to_pppos.c"
                            ${NETWORK_DCE}
                    INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
