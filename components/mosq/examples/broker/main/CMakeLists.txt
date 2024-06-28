idf_component_register(SRCS "broker.c"
                       PRIV_REQUIRES newlib nvs_flash esp_netif esp_event mqtt)
