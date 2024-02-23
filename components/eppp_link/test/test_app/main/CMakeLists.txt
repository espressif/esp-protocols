idf_component_register(SRCS app_main.c
                    INCLUDE_DIRS "."
                    REQUIRES test_utils
                    PRIV_REQUIRES unity nvs_flash esp_netif driver esp_event)
