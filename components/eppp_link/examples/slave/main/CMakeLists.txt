if(CONFIG_EXAMPLE_WIFI_OVER_EPPP_CHANNEL)
    set(wifi_over_channels channel_wifi_station.c)
endif()
idf_component_register(SRCS eppp_slave.c
                            ${wifi_over_channels}
                       INCLUDE_DIRS ".")
