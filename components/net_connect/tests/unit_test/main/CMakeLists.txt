
idf_component_register(SRCS "test_net_connect.c"
                       REQUIRES test_utils esp_netif esp_event
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES unity net_connect)
