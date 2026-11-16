idf_component_register(SRCS "net_iface_events_example.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif nvs_flash esp_event esp_timer lwip)
