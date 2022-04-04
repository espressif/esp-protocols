include($ENV{IDF_PATH}/tools/cmake/version.cmake)
idf_build_get_property(target IDF_TARGET)

# USB is supported on S2 and S3 targets and IDF version >= 4.4
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "4.4")
if(("${target}" STREQUAL "esp32s2") OR ("${target}" STREQUAL "esp32s3"))

idf_component_register(SRCS "esp_modem_usb.cpp" "esp_modem_usb_api_target.cpp"
                       REQUIRES cdc_acm_host esp_modem
                       REQUIRED_IDF_TARGETS esp32s2 esp32s3
                       PRIV_INCLUDE_DIRS "private_include"
                       INCLUDE_DIRS "include")
                    
set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
)

endif()
endif()