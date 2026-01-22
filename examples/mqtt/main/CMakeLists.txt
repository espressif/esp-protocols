idf_component_register(SRCS "app_main.c"
                    INCLUDE_DIRS ".")

                    # target_compile_options(${COMPONENT_LIB} PUBLIC -fsanitize=address)
                    # target_link_options(${COMPONENT_LIB} PUBLIC -fsanitize=address)
                    