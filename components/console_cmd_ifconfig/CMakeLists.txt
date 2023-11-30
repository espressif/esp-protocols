idf_component_register(SRCS "console_ifconfig.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif console esp_eth
                    WHOLE_ARCHIVE)
