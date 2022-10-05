idf_component_register(SRCS "modem_client.cpp"
                            "sock_dce.cpp"
                            "sock_commands.cpp"
                       INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
