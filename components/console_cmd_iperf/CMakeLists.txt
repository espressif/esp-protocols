idf_component_register(SRCS "console_iperf.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif console
                    WHOLE_ARCHIVE)
