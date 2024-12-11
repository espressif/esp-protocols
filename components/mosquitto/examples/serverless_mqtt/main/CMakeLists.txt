idf_component_register(SRCS "serverless_mqtt.c"
                            "wifi_connect.c"
                    INCLUDE_DIRS "."
                    REQUIRES libjuice nvs_flash mqtt json esp_wifi)
