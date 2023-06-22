set(ppp_connect_srcs ppp_connect.c)
if(CONFIG_EXAMPLE_PPP_CONNECT_ESP_MODEM)
    list(APPEND ppp_connect_srcs ppp_connect_esp_modem.c)
else()
    list(APPEND ppp_connect_srcs ppp_connect_simple.c)
endif()

idf_component_register(SRCS multi_netif_main.c
                            check_connection.c
                            wifi_connect.c
                            ethernet_connect.c
                            ${ppp_connect_srcs}
                       INCLUDE_DIRS ".")
