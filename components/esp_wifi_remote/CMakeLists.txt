if(NOT CONFIG_ESP_WIFI_ENABLED)
    set(src_wifi_is_remote esp_wifi_remote.c esp_wifi_with_remote.c)
endif()

if(CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED)
    set(src_wifi_uses_esp_hosted wifi_remote_init.c wifi_remote_rpc.c)
endif()

idf_component_register(INCLUDE_DIRS include
                    SRCS wifi_remote_net.c
                        esp_wifi_remote_weak.c
                        eppp/wifi_remote_impl.cpp
                        eppp/wifi_remote_rpc_client.cpp
                        eppp/wifi_remote_rpc_server.cpp
                        eppp/eppp_init.c
                        ${src_wifi_uses_esp_hosted}
                        ${src_wifi_is_remote}
                    PRIV_INCLUDE_DIRS eppp
                    REQUIRES esp_event esp_netif
                    PRIV_REQUIRES esp_wifi esp-tls)
