if(CONFIG_EPPP_LINK_DEVICE_ETH)
    set(transport_src eppp_eth.c)
endif()

if(CONFIG_EPPP_LINK_DEVICE_SDIO)
    set(transport_src eppp_sdio_slave.c eppp_sdio_host.c)
endif()

idf_component_register(SRCS eppp_link.c ${transport_src}
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_netif esp_driver_spi esp_driver_gpio esp_timer driver esp_eth)

if(CONFIG_EPPP_LINK_DEVICE_ETH)
    idf_component_get_property(ethernet_init espressif__ethernet_init COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PRIVATE ${ethernet_init})
endif()
