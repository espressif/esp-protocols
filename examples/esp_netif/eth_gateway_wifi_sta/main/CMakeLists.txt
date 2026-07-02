idf_component_register(SRCS "eth_gateway_wifi_sta.c"
                       INCLUDE_DIRS "."
                       REQUIRES ethernet_init esp_netif esp_eth esp_wifi nvs_flash)
