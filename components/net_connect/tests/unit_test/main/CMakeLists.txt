
idf_component_register(SRCS "test_net_connect.c"
                       REQUIRES test_utils esp_netif esp_event
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES unity net_connect esp_wifi)

if(CONFIG_NET_CONNECT_THREAD)
    idf_component_optional_requires(PRIVATE openthread)
endif()
