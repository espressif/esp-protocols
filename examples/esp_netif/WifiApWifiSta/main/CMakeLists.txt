idf_component_register(SRCS "WifiApWifiSta.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_netif esp_wifi nvs_flash)
