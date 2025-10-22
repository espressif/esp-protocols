# CSLIP client example

idf_component_register(
    SRCS "cslip_client_main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_netif cslip_modem driver
)
