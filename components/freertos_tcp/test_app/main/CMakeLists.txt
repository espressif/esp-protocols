idf_component_register(SRCS "test_app_main.c"
                              "test_tcp_basic.c"
                              "test_udp_basic.c"
                              "test_dns.c"
                       INCLUDE_DIRS "."
                       REQUIRES unity freertos_tcp esp_wifi console_cmd_wifi nvs_flash esp_event
                       WHOLE_ARCHIVE)
