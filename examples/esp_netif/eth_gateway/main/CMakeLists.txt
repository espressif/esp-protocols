idf_component_register(SRCS "eth_gateway.c"
                       INCLUDE_DIRS "."
                       REQUIRES ethernet_init esp_netif esp_eth nvs_flash)
