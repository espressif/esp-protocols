# CSLIP Modem Component (initial, pass-through)

idf_component_register(
    SRCS "library/cslip_modem.c" "library/cslip_modem_netif.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_netif driver
)
