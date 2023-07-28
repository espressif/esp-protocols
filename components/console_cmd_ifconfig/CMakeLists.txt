idf_component_register(SRCS "console_connect.c" "console_ifconfig.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "ethernet_init"
                    PRIV_REQUIRES esp_netif console nvs_flash esp_eth)
