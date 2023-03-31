set(requires "")
if(${IDF_TARGET} STREQUAL "linux")
    list(APPEND requires esp_stubs esp-tls protocol_examples_common mqtt)
endif()
idf_component_register(SRCS "app_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES ${requires})
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
