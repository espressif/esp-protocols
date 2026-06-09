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

# Custom HTTP header support (lws_hdr_custom_*) is applied as a compile definition
# after add_subdirectory() below: lws's option(LWS_WITH_CUSTOM_HEADERS) is gated
# "H1 only" and gets dropped on the FreeRTOS/minimal profile, so the CMake-option
# route leaves it #undef in lws_config.h. A PUBLIC compile define is reliable.

# ESP picolibc ships timegm() in libc but gates its <time.h> prototype behind
# BSD/GNU visibility (and the symbol isn't reliably linkable), so lws's
# CHECK_FUNCTION_EXISTS(timegm) fails and lws_http_date_parse_unix() falls back to
# mktime() — which applies the local TZ and misreads a server "GMT" Date header by
# the TZ offset (breaks HTTP clock-skew handling, e.g. AWS SigV4). Force-enable and
# supply a UTC timegm() via the port (prototype shim force-included into lws).
set(LWS_HAVE_TIMEGM 1 CACHE INTERNAL "ESP port provides UTC timegm()" FORCE)


set(WRAP_FUNCTIONS mbedtls_ssl_handshake_step
                   lws_adopt_descriptor_vhost)

foreach(wrap ${WRAP_FUNCTIONS})
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=${wrap}")
endforeach()

target_link_libraries(${COMPONENT_LIB} INTERFACE websockets)

target_sources(${COMPONENT_LIB} INTERFACE "port/lws_port.c")

idf_component_get_property(mbedtls_dir mbedtls COMPONENT_DIR)

# lws's mbedtls 4 detection probe is gated by `if (LWS_MBEDTLS_INCLUDE_DIRS)`
# in lib/tls/mbedtls/CMakeLists.txt:120, which on FreeRTOS is empty (the
# find_path call only runs on non-FreeRTOS). Result: on ESP-IDF, lws never
# sets `LWS_HAVE_MBEDTLS_V4` even when the mbedtls submodule is v4 — and
# lws's mbedtls-4-aware source (libwebsockets.h:414 etc.) goes down the
# v3 branch and breaks. Pre-set the cache var when we know IDF mbedtls is
# v4 so lws picks the right branch. Verified empirically against IDF v6.
file(STRINGS "${mbedtls_dir}/mbedtls/include/mbedtls/build_info.h"
     _mbedtls_major REGEX "^#define MBEDTLS_VERSION_MAJOR[ \t]+[0-9]+")
if(_mbedtls_major MATCHES "[ \t]+([0-9]+)" AND CMAKE_MATCH_1 GREATER_EQUAL 4)
    set(LWS_HAVE_MBEDTLS_V4 1 CACHE INTERNAL "mbedtls 4 detected on IDF")
endif()

add_subdirectory(libwebsockets)

# pollfd.c at this lws pin has an unused 'vpt' on FreeRTOS; drop after the next bump.
target_compile_options(websockets PRIVATE -Wno-unused-variable)

# Supply a UTC timegm() for lws (LWS_HAVE_TIMEGM set above). lws_http_date_parse_unix()
# calls timegm(); the port source defines it and the shim force-includes a prototype
# so date.c links against it instead of an implicit (int-returning) declaration.
target_sources(websockets PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/port/lws_timegm.c")
target_compile_options(websockets PRIVATE "-include${CMAKE_CURRENT_SOURCE_DIR}/port/lws_timegm_shim.h")

# Enable custom HTTP header read APIs (lws_hdr_custom_*). PUBLIC so both lws's own
# sources and consumers (e.g. KVS WebRTC signaling) see the macro. Reliable where
# the lws CMake option is dropped on the FreeRTOS/minimal profile.
if(CONFIG_LWS_WITH_CUSTOM_HEADERS)
    target_compile_definitions(websockets PUBLIC LWS_WITH_CUSTOM_HEADERS=1 LWS_WITH_CUSTOM_HTTP_HEADERS=1)
endif()

# mbedTLS 4 (IDF v6) split public headers across tf-psa-crypto/. The inner
# `websockets` target reaches mbedtls headers via lws's hardcoded
# $ENV{IDF_PATH}/components/mbedtls/... list (lib/CMakeLists.txt:60), which
# only mentions mbedtls/include + port/include. Inject the new dirs here
# when present so the IDF v6 (mbedtls 4) chain resolves; the EXISTS guard
# keeps IDF v5.x (mbedtls 3) builds untouched.
# Drop once upstream lws covers tf-psa-crypto in its ESP_PLATFORM block.
if(EXISTS "${mbedtls_dir}/mbedtls/tf-psa-crypto/include")
    target_include_directories(websockets PRIVATE
        "${mbedtls_dir}/mbedtls/tf-psa-crypto/include"
        "${mbedtls_dir}/mbedtls/tf-psa-crypto/drivers/builtin/include"
        "${mbedtls_dir}/port/psa_driver/include")
endif()
