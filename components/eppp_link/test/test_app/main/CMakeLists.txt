if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.3")
    set(driver_deps esp_driver_gpio esp_driver_spi esp_driver_uart esp_driver_sdio)
else()
    set(driver_deps driver)
endif()

idf_component_register(SRCS app_main.c
                    INCLUDE_DIRS "."
                    REQUIRES test_utils
                    PRIV_REQUIRES unity nvs_flash esp_netif esp_event ${driver_deps})
