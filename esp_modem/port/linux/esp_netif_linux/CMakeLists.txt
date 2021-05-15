set(LWIP_DIR "$ENV{LWIP_PATH}")
set(LWIP_CONTRIB_DIR "$ENV{LWIP_CONTRIB_PATH}")

set(lwipcontribportunix_SRCS  ${LWIP_CONTRIB_DIR}/ports/unix/port/sys_arch.c)

include(${LWIP_DIR}/src/Filelists.cmake)

set (LWIP_INCLUDE_DIRS
        "${LWIP_DIR}/src/include"
        "${LWIP_CONTRIB_DIR}/ports/unix/port/include")

list(REMOVE_ITEM lwipnoapps_SRCS "${LWIP_DIR}/src/netif/slipif.c")
list(REMOVE_ITEM lwipnoapps_SRCS "${LWIP_DIR}/src/core/ip4.c")
list(REMOVE_ITEM lwipnoapps_SRCS "${LWIP_DIR}/src/core/ipv6/ip6.c")


idf_component_register(SRCS esp_netif_linux.cpp tun_io.c ip6_stub.c ${lwipnoapps_SRCS} ${lwipcontribportunix_SRCS}
                       INCLUDE_DIRS include ${LWIP_INCLUDE_DIRS}
                       PRIV_INCLUDE_DIRS .
                       REQUIRES esp_system_protocols_linux)

