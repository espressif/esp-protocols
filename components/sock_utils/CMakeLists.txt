idf_component_register(SRCS "src/getnameinfo.c"
                            "src/ifaddrs.c"
                            "src/gai_strerror.c"
                            "src/socketpair.c"
                            "src/gethostname.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES lwip esp_netif)

# To support declarations from standard headers in lwip component
# - socket pair from lwip/sockets.h
# - gai_strerror from lwip/netdb.h
# also need to make lwip depend on the sock_utils lib
idf_component_get_property(lwip lwip COMPONENT_LIB)
target_compile_definitions(${lwip} PUBLIC LWIP_SOCKET_HAS_SOCKETPAIR=1)
target_compile_definitions(${lwip} PUBLIC LWIP_NETDB_HAS_GAI_STRERROR=1)
target_link_libraries(${lwip} PUBLIC ${COMPONENT_LIB})
