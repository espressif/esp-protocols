if(CONFIG_EXAMPLE_PEER_LIB_ESP_PEER)
    set(PEER_BACKEND_SRC "peer_impl_webrtc.c")
else()
    set(PEER_BACKEND_SRC "peer_impl_juice.c")
endif()

idf_component_register(SRCS "serverless_mqtt.c"
                            "wifi_connect.c"
                            "${PEER_BACKEND_SRC}"
                    INCLUDE_DIRS "."
                    REQUIRES libjuice nvs_flash mqtt json esp_wifi)
if(CONFIG_EXAMPLE_PEER_LIB_ESP_PEER)
    idf_component_optional_requires(PUBLIC media_lib_sal esp_webrtc peer_default)
endif()
