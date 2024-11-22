idf_component_register(SRCS "main.c" "esp_system_linux2.c"
                    INCLUDE_DIRS
                    "."
                    REQUIRES mdns console nvs_flash)
