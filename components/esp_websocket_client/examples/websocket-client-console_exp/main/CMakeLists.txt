idf_component_register(SRCS "websocket_client_vfs.c"
                            "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES console vfs nvs_flash esp_event esp_netif esp_wifi esp_ringbuf esp_eth)
