idf_component_register(SRCS "EthAp.c"
                       INCLUDE_DIRS "."
                       REQUIRES ethernet_init esp_netif esp_eth nvs_flash)
