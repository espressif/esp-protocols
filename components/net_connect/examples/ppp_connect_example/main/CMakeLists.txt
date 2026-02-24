idf_component_register(SRCS "ppp_connect_example.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif nvs_flash esp_event)
