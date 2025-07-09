idf_component_register(SRCS "serverless_mqtt.c"
                            "wifi_connect.c"
                            "webrtc.c"
                    INCLUDE_DIRS "."
                    REQUIRES libjuice nvs_flash mqtt json esp_wifi media_lib_sal esp_webrtc peer_default)
