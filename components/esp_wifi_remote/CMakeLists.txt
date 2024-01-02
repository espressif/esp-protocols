if(NOT CONFIG_ESP_WIFI_ENABLED)
    set(src_wifi_is_remote esp_wifi_remote.c)
endif()

idf_component_register(INCLUDE_DIRS include
                    SRCS wifi_remote_rpc.c wifi_remote_net.c wifi_remote_init.c ${src_wifi_is_remote}
                    REQUIRES esp_event esp_netif
                    PRIV_REQUIRES esp_wifi esp_hosted)
