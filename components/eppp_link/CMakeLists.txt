if(CONFIG_EPPP_LINK_USB_CDC_DEVICE)
    set(transport_srcs eppp_usb_device.c)
elseif(CONFIG_EPPP_LINK_USB_CDC_HOST)
    set(transport_srcs eppp_usb_host.c)
endif()

idf_component_register(SRCS eppp_link.c eppp_sdio_slave.c eppp_sdio_host.c ${transport_srcs}
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_netif esp_driver_spi esp_driver_gpio esp_timer driver)

if(CONFIG_EPPP_LINK_USB_CDC_DEVICE)
    idf_component_optional_requires(PRIVATE espressif__esp_tinyusb)
endif()
