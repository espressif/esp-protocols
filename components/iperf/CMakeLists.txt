idf_component_register(SRCS "iperf.c"
                            "console_iperf.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_netif console esp_timer)

if(CONFIG_IPERF_CMD_AUTO_REGISTRATION)
    target_link_libraries(${COMPONENT_LIB} "-u console_cmd_iperf_register")
endif()
