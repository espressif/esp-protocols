idf_component_register(REQUIRES mbedtls)

set(LWS_WITH_EXPORT_LWSTARGETS OFF CACHE BOOL "Export libwebsockets CMake targets.  Disable if they conflict with an outer cmake project.")
set(LWS_WITH_MBEDTLS ON CACHE BOOL "Use mbedTLS (>=2.0) replacement for OpenSSL.")
set(LWS_WITH_JPEG OFF CACHE BOOL "Enable stateful JPEG stream decoder")


set(WRAP_FUNCTIONS mbedtls_ssl_handshake_step
                   lws_adopt_descriptor_vhost)

foreach(wrap ${WRAP_FUNCTIONS})
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=${wrap}")
endforeach()

target_link_libraries(${COMPONENT_LIB} INTERFACE websockets)

target_sources(${COMPONENT_LIB} INTERFACE "port/lws_port.c")


add_subdirectory(libwebsockets)
