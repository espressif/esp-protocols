idf_component_register(SRCS "console_mqtt.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif console mqtt)

target_link_libraries(${COMPONENT_LIB} PRIVATE "-u console_cmd_mqtt_register")
