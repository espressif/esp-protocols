if(CONFIG_MDNS_NETWORKING_SOCKET)
    set(MDNS_NETWORKING "mdns_networking_socket.c")
else()
    set(MDNS_NETWORKING "mdns_networking_lwip.c")
endif()

if(CONFIG_MDNS_ENABLE_CONSOLE_CLI)
    set(MDNS_CONSOLE "mdns_console.c")
else()
    set(MDNS_CONSOLE "")
endif()

set(MDNS_MEMORY "mdns_mem_caps.c")

set(MDNS_CORE "mdns_responder.c" "mdns_receive.c" "mdns_utils.c" "mdns_debug.c" "mdns_send.c" "mdns_netif.c"
                "mdns_querier.c" "mdns_pcb.c" "mdns_service.c")

if(CONFIG_MDNS_ENABLE_BROWSE)
    list(APPEND MDNS_CORE "mdns_browser.c")
endif()

idf_build_get_property(target IDF_TARGET)
if(${target} STREQUAL "linux")
    set(dependencies esp_netif_linux esp_event)
    set(private_dependencies esp_timer console esp_system)
    set(srcs ${MDNS_CORE} ${MDNS_MEMORY} ${MDNS_NETWORKING} ${MDNS_CONSOLE})
else()
    set(dependencies lwip console esp_netif)
    set(private_dependencies esp_timer)
    set(srcs ${MDNS_CORE} ${MDNS_MEMORY} ${MDNS_NETWORKING} ${MDNS_CONSOLE})
endif()

idf_component_register(
        SRCS ${srcs}
        INCLUDE_DIRS "include"
        PRIV_INCLUDE_DIRS "private_include"
        REQUIRES ${dependencies}
        PRIV_REQUIRES ${private_dependencies})

if(${target} STREQUAL "linux")
    target_link_libraries(${COMPONENT_LIB} PRIVATE "-lbsd")
endif()

if(CONFIG_ETH_ENABLED)
    idf_component_optional_requires(PRIVATE esp_eth)
endif()

# esp_wifi is only used by the predefined STA/AP interface event handlers in
# mdns_netif.c; mirror the MDNS_ESP_WIFI_ENABLED condition used there so
# builds without Wi-Fi do not pull the Wi-Fi stack into the build.
if(NOT ${target} STREQUAL "linux")
    if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.1")
        if(CONFIG_ESP_WIFI_ENABLED OR CONFIG_ESP_WIFI_REMOTE_ENABLED)
            idf_component_optional_requires(PRIVATE esp_wifi)
        endif()
    elseif(CONFIG_SOC_WIFI_SUPPORTED)
        # ESP_WIFI_ENABLED is not available on IDF <= 5.1
        idf_component_optional_requires(PRIVATE esp_wifi)
    endif()
endif()

idf_component_get_property(MDNS_VERSION ${COMPONENT_NAME} COMPONENT_VERSION)
target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DESP_MDNS_VERSION_NUMBER=\"${MDNS_VERSION}\"")
