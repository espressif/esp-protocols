idf_component_register(REQUIRES esp-tls)

option(LWS_WITH_EXPORT_LWSTARGETS "Export libwebsockets CMake targets.  Disable if they conflict with an outer cmake project." OFF)
set(LWS_WITH_EXPORT_LWSTARGETS OFF)

option(LWS_WITH_MBEDTLS "Use mbedTLS (>=2.0) replacement for OpenSSL. When setting this, you also may need to specify LWS_MBEDTLS_LIBRARIES and LWS_MBEDTLS_INCLUDE_DIRS" ON)
set(LWS_WITH_MBEDTLS ON)

set(WRAP_FUNCTIONS mbedtls_ssl_handshake_step
                   lws_adopt_descriptor_vhost)

foreach(wrap ${WRAP_FUNCTIONS})
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=${wrap}")
endforeach()

target_link_libraries(${COMPONENT_LIB} INTERFACE websockets)

target_sources(${COMPONENT_LIB} INTERFACE "port/lws_port.c")


add_subdirectory(libwebsockets)
