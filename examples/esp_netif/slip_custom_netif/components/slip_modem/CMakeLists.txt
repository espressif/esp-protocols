# SLIP Modem Component
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.3")
    set(dependencies esp_driver_uart)
else()
    set(dependencies driver)
endif()

idf_component_register(
    SRCS "library/slip_modem.c" "library/slip_modem_netif.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_netif ${dependencies}
)
