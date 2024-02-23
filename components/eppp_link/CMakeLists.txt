idf_component_register(SRCS "eppp_link.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_netif esp_driver_spi esp_driver_gpio esp_timer driver)
