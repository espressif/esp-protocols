idf_component_register(REQUIRES mbedtls)

set(LWS_WITH_EXPORT_LWSTARGETS OFF CACHE BOOL "Export libwebsockets CMake targets.  Disable if they conflict with an outer cmake project.")
set(LWS_WITH_MBEDTLS ON CACHE BOOL "Use mbedTLS (>=2.0) replacement for OpenSSL.")
set(LWS_WITH_JPEG OFF CACHE BOOL "Enable stateful JPEG stream decoder")

# Kconfig-driven libwebsockets options. Defaults preserve the previous
# behaviour (extensions off, file_ops on, threadpool off) so existing
# consumers see no change. See Kconfig for help text and per-knob rationale.
if(CONFIG_LWS_WITH_EXTENSIONS)
    set(LWS_WITHOUT_EXTENSIONS OFF CACHE INTERNAL "Compile with extensions")
else()
    set(LWS_WITHOUT_EXTENSIONS ON  CACHE INTERNAL "Don't compile with extensions")
endif()

if(CONFIG_LWS_WITH_FILE_OPS)
    set(LWS_WITH_FILE_OPS ON  CACHE INTERNAL "Support file operations vfs")
else()
    set(LWS_WITH_FILE_OPS OFF CACHE INTERNAL "Drop file operations vfs")
endif()

if(CONFIG_LWS_WITH_THREADPOOL)
    set(LWS_WITH_THREADPOOL ON  CACHE INTERNAL "Managed worker thread pool support")
else()
    set(LWS_WITH_THREADPOOL OFF CACHE INTERNAL "No worker thread pool")
endif()


set(WRAP_FUNCTIONS mbedtls_ssl_handshake_step
                   lws_adopt_descriptor_vhost)

foreach(wrap ${WRAP_FUNCTIONS})
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=${wrap}")
endforeach()

target_link_libraries(${COMPONENT_LIB} INTERFACE websockets)

target_sources(${COMPONENT_LIB} INTERFACE "port/lws_port.c")


add_subdirectory(libwebsockets)

# pollfd.c at this lws pin has an unused 'vpt' on FreeRTOS; drop after the next bump.
target_compile_options(websockets PRIVATE -Wno-unused-variable)
