if(CONFIG_EXAMPLE_PEER_LIB_ESP_PEER)
    set(PEER_BACKEND_SRC "webrtc.c")
endif()

idf_component_register(SRCS "serverless_mqtt.c" "peer_impl.c"
                            "wifi_connect.c"
                            "${PEER_BACKEND_SRC}"
                    INCLUDE_DIRS "."
                    REQUIRES libjuice nvs_flash mqtt json esp_wifi)
if(CONFIG_EXAMPLE_PEER_LIB_ESP_PEER)
    idf_component_optional_requires(PUBLIC media_lib_sal esp_webrtc peer_default)
endif()
