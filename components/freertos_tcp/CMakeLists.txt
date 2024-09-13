set(fdir  $ENV{IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/freertos)
set(fcdir $ENV{IDF_PATH}/components/freertos/config/include/freertos)

set(fpt_dir FreeRTOS-Plus-TCP/source)
set(fpt_include_dir ${fpt_dir}/include ${fpt_dir}/portable/Compiler/GCC/)

set(fpt_srcs ${fpt_dir}/FreeRTOS_ARP.c
        ${fpt_dir}/FreeRTOS_BitConfig.c
        ${fpt_dir}/FreeRTOS_DHCP.c
        ${fpt_dir}/FreeRTOS_DHCPv6.c
        ${fpt_dir}/FreeRTOS_DNS.c
        ${fpt_dir}/FreeRTOS_DNS_Cache.c
        ${fpt_dir}/FreeRTOS_DNS_Callback.c
        ${fpt_dir}/FreeRTOS_DNS_Networking.c
        ${fpt_dir}/FreeRTOS_DNS_Parser.c
        ${fpt_dir}/FreeRTOS_ICMP.c
        ${fpt_dir}/FreeRTOS_IP.c
        ${fpt_dir}/FreeRTOS_IP_Timers.c
        ${fpt_dir}/FreeRTOS_IP_Utils.c
        ${fpt_dir}/FreeRTOS_IPv4.c
        ${fpt_dir}/FreeRTOS_IPv4_Sockets.c
        ${fpt_dir}/FreeRTOS_IPv4_Utils.c
        ${fpt_dir}/FreeRTOS_IPv6.c
        ${fpt_dir}/FreeRTOS_IPv6_Sockets.c
        ${fpt_dir}/FreeRTOS_IPv6_Utils.c
        ${fpt_dir}/FreeRTOS_ND.c
        ${fpt_dir}/FreeRTOS_RA.c
        ${fpt_dir}/FreeRTOS_Routing.c
        ${fpt_dir}/FreeRTOS_Sockets.c
        ${fpt_dir}/FreeRTOS_Stream_Buffer.c
        ${fpt_dir}/FreeRTOS_TCP_IP.c
        ${fpt_dir}/FreeRTOS_TCP_IP_IPv4.c
        ${fpt_dir}/FreeRTOS_TCP_IP_IPv6.c
        ${fpt_dir}/FreeRTOS_TCP_Reception.c
        ${fpt_dir}/FreeRTOS_TCP_State_Handling.c
        ${fpt_dir}/FreeRTOS_TCP_State_Handling_IPv4.c
        ${fpt_dir}/FreeRTOS_TCP_State_Handling_IPv6.c
        ${fpt_dir}/FreeRTOS_TCP_Transmission.c
        ${fpt_dir}/FreeRTOS_TCP_Transmission_IPv4.c
        ${fpt_dir}/FreeRTOS_TCP_Transmission_IPv6.c
        ${fpt_dir}/FreeRTOS_TCP_Utils.c
        ${fpt_dir}/FreeRTOS_TCP_Utils_IPv4.c
        ${fpt_dir}/FreeRTOS_TCP_Utils_IPv6.c
        ${fpt_dir}/FreeRTOS_TCP_WIN.c
        ${fpt_dir}/FreeRTOS_Tiny_TCP.c
        ${fpt_dir}/FreeRTOS_UDP_IP.c
        ${fpt_dir}/FreeRTOS_UDP_IPv4.c
        ${fpt_dir}/FreeRTOS_UDP_IPv6.c
        ${fpt_dir}/portable/BufferManagement/BufferAllocation_1.c
        )

idf_component_register(SRCS ${fpt_srcs}
                    port/FreeRTOSIPConfig.c
                    port/NetworkInterface.c
                    port/FreeRTOS_AppHooks.c
                            esp_netif/interface.c
                            esp_netif/esp_netif_impl.c
                    INCLUDE_DIRS "port/include" ${fpt_include_dir} ${fdir}
                    PRIV_INCLUDE_DIRS  ${fcdir}
                    PRIV_REQUIRES esp_wifi esp_netif)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
target_compile_definitions(${COMPONENT_LIB} PUBLIC "FREERTOS_CONFIG_H")
target_link_libraries(${COMPONENT_LIB} PRIVATE "-u _g_esp_netif_netstack_default_wifi_sta")
