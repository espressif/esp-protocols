if(CONFIG_EXAMPLE_WIFI_OVER_EPPP_CHANNEL)
    set(wifi_over_channels channel_wifi_station.c)
endif()
idf_component_register(SRCS app_main.c register_iperf.c
                            ${wifi_over_channels}
                       INCLUDE_DIRS ".")
