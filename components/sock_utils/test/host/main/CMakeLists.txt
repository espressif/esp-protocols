idf_component_register(SRCS "test_sock_utils.cpp"
                        INCLUDE_DIRS "$ENV{IDF_PATH}/tools"
                        WHOLE_ARCHIVE)

target_compile_options(${COMPONENT_LIB} PUBLIC -fsanitize=address -fconcepts)
target_link_options(${COMPONENT_LIB} PUBLIC -fsanitize=address)
