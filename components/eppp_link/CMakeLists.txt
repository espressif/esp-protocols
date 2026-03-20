if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.3")
    set(driver_deps esp_driver_gpio esp_driver_spi esp_driver_uart esp_driver_sdio)
else()
    set(driver_deps driver)
endif()

if(CONFIG_EPPP_LINK_DEVICE_ETH)
    set(transport_src eppp_eth.c)
endif()

if(CONFIG_EPPP_LINK_DEVICE_SPI)
    set(transport_src eppp_spi.c)
endif()

if(CONFIG_EPPP_LINK_DEVICE_UART)
    set(transport_src eppp_uart.c)
endif()

if(CONFIG_EPPP_LINK_DEVICE_SDIO)
    set(transport_src eppp_sdio.c eppp_sdio_slave.c eppp_sdio_host.c)
endif()

if(NOT CONFIG_EPPP_LINK_USES_PPP)
    set(netif_src eppp_netif_tun.c)
endif()

idf_component_register(SRCS eppp_link.c ${transport_src} ${netif_src}
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_netif esp_timer esp_eth ${driver_deps})

if(CONFIG_EPPP_LINK_DEVICE_ETH)
    idf_component_optional_requires(PRIVATE ethernet_init espressif__ethernet_init)
endif()
