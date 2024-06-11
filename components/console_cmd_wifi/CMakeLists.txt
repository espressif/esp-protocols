idf_component_register(SRCS "console_wifi.c"
                    INCLUDE_DIRS "include"
                    REQUIRES lwip
                    PRIV_REQUIRES esp_netif console esp_wifi esp_timer)

if(CONFIG_WIFI_CMD_AUTO_REGISTRATION)
    target_link_libraries(${COMPONENT_LIB} "-u console_cmd_wifi_register")
endif()
