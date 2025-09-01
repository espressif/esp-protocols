idf_component_register(SRCS "example_broker.c"
                       PRIV_REQUIRES nvs_flash esp_netif esp_event mqtt
                       EMBED_TXTFILES servercert.pem serverkey.pem cacert.pem)
