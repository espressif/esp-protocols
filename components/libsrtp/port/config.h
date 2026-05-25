/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP-IDF build configuration for libsrtp. Define values are chosen to
 * match the ESP-IDF/newlib environment; format follows libsrtp's
 * upstream config.h.in template (BSD-3-Clause).
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <inttypes.h>  // For PRIx64 / PRIu64 macros (needed on Apple clang / Linux target)
#include <stddef.h>    // For size_t definition

/*
 * CPU architecture hint. libsrtp uses this only to select a fast endian-flip
 * path in a few places; the C fallback is correct on all targets. ESP-IDF's
 * supported targets are Xtensa (esp32 / s2 / s3) and RISC-V (c3 / c5 / c6 /
 * p4 / h2), so CPU_CISC would be wrong here. Set CPU_RISC for the embedded
 * targets and leave it unset (= C fallback) on the IDF Linux host target.
 */
#if defined(__XTENSA__) || defined(__riscv)
#define CPU_RISC 1
#endif

/* Report errors to stdout. */
#define ERR_REPORTING_STDOUT 1

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you want to use external crypto. */
#define SRTP_CRYPTO_MBEDTLS 1

/* Define package info */
#define PACKAGE_NAME "libsrtp"
#define PACKAGE_VERSION "2.8.0"
#define PACKAGE_STRING PACKAGE_NAME " " PACKAGE_VERSION

/* The size of `unsigned long', as computed by sizeof. */
#define SIZEOF_UNSIGNED_LONG 4

/* The size of `unsigned long long', as computed by sizeof. */
#define SIZEOF_UNSIGNED_LONG_LONG 8

/* Force SRTP to use system types */
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UINT64_T 1
#define HAVE_UINT32_T 1
#define HAVE_UINT16_T 1
#define HAVE_UINT8_T 1
#define HAVE_INT64_T 1
#define HAVE_INT32_T 1
#define HAVE_INT16_T 1
#define HAVE_INT8_T 1

/* Prevent SRTP from redefining types */
#define INTEGERS_H

/* Prevent srtp_ssrc_t redefinition */
#ifdef __XTENSA__
#define SRTP_KERNEL_COMPAT
#endif

/* Prevent in_addr_t redefinition */
#ifndef _LWIP_INET_H
typedef uint32_t in_addr_t;
#endif

#define GCM 1
#define MBEDTLS 1
#define ERR_REPORTING_STDOUT 1

#endif /* CONFIG_H */
