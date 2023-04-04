idf_component_register(SRCS "app_main.cpp"
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")

if(${IDF_TARGET} STREQUAL "linux")
    if(WITH_LWIP STREQUAL "1")
        target_compile_definitions(${COMPONENT_LIB} PUBLIC WITH_LWIP)
    endif()
endif()
