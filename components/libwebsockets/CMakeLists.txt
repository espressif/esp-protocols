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

# Stop lws adding its own -Werror (the IDF build already sets -Werror globally).
set(DISABLE_WERROR ON CACHE BOOL "lws: don't treat warnings as errors" FORCE)

add_subdirectory(libwebsockets)

# The IDF build compiles everything with a global -Werror.  IDF v6's mbedtls
# headers emit a harmless MBEDTLS_PSA_BUILTIN_ALG_HMAC redefinition warning
# (esp_config.h vs tf-psa-crypto) that lws's sources pull in via libwebsockets.h
# -> mbedtls/ssl.h; -Wno-error keeps that (and the unused 'vpt' in this pin's
# pollfd.c) from failing the build.  See esp-idf esp_config.h redefinition bug.
target_compile_options(websockets PRIVATE -Wno-unused-variable -Wno-error)

# IDF v6's libc (picolibc) makes errno thread-local. With lws's LWIP_PROVIDE_ERRNO
# now gated off on v6, lwip's errno.h forwards to the system errno - but lws's
# hardcoded include list (lib/CMakeLists.txt) puts .../lwip/src/include/lwip on the
# path, so a bare <errno.h> resolves to lwip's own errno.h (LWIP_ERRNO_STDINCLUDE
# would re-include itself). Point lwip at <sys/errno.h> instead, which is not
# shadowed and pulls picolibc's TLS errno + E* macros via esp_libc's #include_next.
# Otherwise lws objects fail with "errno undeclared" (compile) or "TLS definition
# ... mismatches non-TLS reference" (link).
target_compile_definitions(websockets PRIVATE "LWIP_ERRNO_INCLUDE=<sys/errno.h>")

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

# The include workarounds below reach into components that only exist on
# IDF v6 (esp_hal_mspi, esp_blockdev, esp_libc). idf_component_get_property
# hard-errors if the component isn't in the build, so gate every lookup on
# BUILD_COMPONENTS membership first - that keeps IDF v5.x untouched.
idf_build_get_property(_lws_build_comps BUILD_COMPONENTS)

# lib/misc/romfs.c pulls esp_flash.h, whose IDF v6 include chain spans
# components split out in v6 (esp_hal_mspi for hal/spi_flash_types.h,
# esp_blockdev) that aren't on lws's hardcoded include list. Add their include
# dirs (not link, which would re-inherit IDF's -Werror).
foreach(_lws_extra_comp esp_hal_mspi esp_blockdev)
    if(_lws_extra_comp IN_LIST _lws_build_comps)
        idf_component_get_property(_lws_cdir ${_lws_extra_comp} COMPONENT_DIR)
        if(_lws_cdir AND EXISTS "${_lws_cdir}/include")
            target_include_directories(websockets PRIVATE "${_lws_cdir}/include")
        endif()
    endif()
endforeach()

# lws's hardcoded ESP include list still points at the legacy 'newlib'
# component, whose non-TLS errno.h shadows IDF v6's esp_libc (picolibc) TLS
# errno - which then fails to link ("TLS definition ... mismatches non-TLS
# reference in async-dns.c.obj"). Prepend esp_libc's platform_include so its
# TLS errno.h wins.
if(esp_libc IN_LIST _lws_build_comps)
    idf_component_get_property(_esp_libc_dir esp_libc COMPONENT_DIR)
    if(_esp_libc_dir AND EXISTS "${_esp_libc_dir}/platform_include")
        target_include_directories(websockets BEFORE PRIVATE "${_esp_libc_dir}/platform_include")
    endif()
endif()
