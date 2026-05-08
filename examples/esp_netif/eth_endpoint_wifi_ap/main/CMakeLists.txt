idf_component_register(SRCS "eth_endpoint_wifi_ap.c"
                       INCLUDE_DIRS "."
                       REQUIRES ethernet_init esp_netif esp_eth esp_wifi nvs_flash)
