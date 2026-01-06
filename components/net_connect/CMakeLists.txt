idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    # Header only library for linux

    idf_component_register(INCLUDE_DIRS include)
    return()
endif()

set(srcs "stdin_out.c"
         "connect.c"
         "wifi_connect.c")

if(CONFIG_NET_CONNECT_PROVIDE_WIFI_CONSOLE_CMD)
    list(APPEND srcs "console_cmd.c")
endif()

if(CONFIG_NET_CONNECT_ETHERNET)
    list(APPEND srcs "eth_connect.c")
endif()

if(CONFIG_NET_CONNECT_THREAD)
    list(APPEND srcs "thread_connect.c")
endif()

if(CONFIG_NET_CONNECT_PPP)
    list(APPEND srcs "ppp_connect.c")
endif()

set(priv_requires esp_netif esp_driver_uart esp_wifi vfs)

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ${priv_requires})

if(CONFIG_NET_CONNECT_PROVIDE_WIFI_CONSOLE_CMD)
    idf_component_optional_requires(PRIVATE console)
endif()

if(CONFIG_NET_CONNECT_ETHERNET)
    idf_component_optional_requires(PRIVATE esp_eth esp_driver_gpio)
endif()

if(CONFIG_NET_CONNECT_THREAD)
    idf_component_optional_requires(PRIVATE openthread)
endif()

if(CONFIG_NET_CONNECT_PPP_DEVICE_USB)
    idf_component_optional_requires(PRIVATE esp_tinyusb espressif__esp_tinyusb)
endif()
