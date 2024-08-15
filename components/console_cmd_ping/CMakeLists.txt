idf_component_register(SRCS "console_ping.c" "console_getaddrinfo.c" "console_getsetdnsserver.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif console)

if(CONFIG_PING_CMD_AUTO_REGISTRATION)
    target_link_libraries(${COMPONENT_LIB} "-u console_cmd_ping_register")
    target_link_libraries(${COMPONENT_LIB} "-u console_cmd_getaddrinfo_register")
    target_link_libraries(${COMPONENT_LIB} "-u console_cmd_setdnsserver_register")
    target_link_libraries(${COMPONENT_LIB} "-u console_cmd_getdnsserver_register")
endif()
