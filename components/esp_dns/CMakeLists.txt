idf_component_register(SRCS
                    "esp_dns_udp.c"
                    "esp_dns_tcp.c"
                    "esp_dns_dot.c"
                    "esp_dns_doh.c"
                    "esp_dns.c"
                    "esp_dns_lwip.c"
                    "esp_dns_utils.c"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "."
                    PRIV_REQUIRES nvs_flash lwip esp_event esp-tls esp_http_client esp-tls tcp_transport)

if(CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM)
    target_link_libraries(${COMPONENT_LIB} "-u lwip_hook_netconn_external_resolve")
endif()
