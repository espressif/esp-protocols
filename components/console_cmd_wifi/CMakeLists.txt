idf_component_register(SRCS "console_wifi.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif console esp_wifi
                    WHOLE_ARCHIVE)
