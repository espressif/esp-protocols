idf_component_register(SRCS "net_connect_example.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif nvs_flash esp_event net_connect)
