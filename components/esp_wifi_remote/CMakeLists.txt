if(NOT CONFIG_ESP_WIFI_ENABLED)
    set(src_wifi_is_remote esp_wifi_remote.c esp_wifi_with_remote.c)
endif()

idf_component_register(INCLUDE_DIRS include
                    SRCS  ${src_wifi_is_remote}
                          esp_wifi_remote_net.c
                          esp_wifi_remote_weak.c
                    REQUIRES esp_event esp_netif
                    PRIV_REQUIRES esp_wifi)

idf_component_optional_requires(PRIVATE esp_hosted)

idf_component_get_property(wifi esp_wifi COMPONENT_LIB)
target_link_libraries(${wifi} PUBLIC ${COMPONENT_LIB})
