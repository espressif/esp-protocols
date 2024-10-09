idf_component_register(SRCS "src/getnameinfo.c"
                            "src/ifaddrs.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES lwip esp_netif)
