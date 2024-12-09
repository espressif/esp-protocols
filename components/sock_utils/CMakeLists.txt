idf_component_register(SRCS "src/getnameinfo.c"
                            "src/ifaddrs.c"
                            "src/gai_strerror.c"
                            "src/socketpair.c"
                            "src/gethostname.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES lwip esp_netif)
