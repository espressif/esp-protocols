idf_component_register(SRCS "serverless_mqtt.c"
                            "wifi_connect.c"
                    INCLUDE_DIRS "."
                    REQUIRES libjuice nvs_flash mqtt json esp_wifi)

if(PEER1)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "PEER1")
elseif(PEER2)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "PEER2")
else()
    message(FATAL_ERROR "Please define either PEER1 or PEER2")
endif()
