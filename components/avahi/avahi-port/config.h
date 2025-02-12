/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: LGPL-2.1
 */
/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Group for running the avahi-autoipd daemon */
#define AVAHI_AUTOIPD_GROUP "avahi-autoipd"

/* User for running the avahi-autoipd daemon */
#define AVAHI_AUTOIPD_USER "avahi-autoipd"

/* Group for Avahi */
#define AVAHI_GROUP "avahi"

/* Privileged access group for Avahi clients */
#define AVAHI_PRIV_ACCESS_GROUP "netdev"

/* User for running the Avahi daemon */
#define AVAHI_USER "avahi"

/* Enable chroot() usage */
/* #undef ENABLE_CHROOT */

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
//#define ENABLE_NLS 0

/* Define if SSP C support is enabled. */
//#define ENABLE_SSP_CC 0

/* Define if SSP C++ support is enabled. */
//#define ENABLE_SSP_CXX 0

/* Gettext package */
#define GETTEXT_PACKAGE "avahi"

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the <bsdxml.h> header file. */
/* #undef HAVE_BSDXML_H */

/* Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Define to 1 if your system has a working `chown' function. */
//#define HAVE_CHOWN 0

/* Define to 1 if you have the `chroot' function. */
//#define HAVE_CHROOT 0

/* Support for DBM */
/* #undef HAVE_DBM */

/* Whether we have D-Bus or not */
//#define HAVE_DBUS 0

/* Define to 1 if you have the `dbus_bus_get_private' function. */
//#define HAVE_DBUS_BUS_GET_PRIVATE 0

/* Define to 1 if you have the `dbus_connection_close' function. */
//#define HAVE_DBUS_CONNECTION_CLOSE 0

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
//#define HAVE_DCGETTEXT 0

/* Define to 1 if you have the declaration of `environ', and to 0 if you
   don't. */
//#define HAVE_DECL_ENVIRON 0

/* Define to 1 if you have the <dlfcn.h> header file. */
//#define HAVE_DLFCN_H 0

/* Have dlopen() */
//#define HAVE_DLOPEN 0

/* Define to 1 if you have the <expat.h> header file. */
//#define HAVE_EXPAT_H 0

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define if you have gcc -fvisibility=hidden support */
//#define HAVE_GCC_VISIBILITY 0

/* Support for GDBM */
#define HAVE_GDBM /**/

/* Define to 1 if you have the <gdbm.h> header file. */
//#define HAVE_GDBM_H 0

/* Define to 1 if you have the `gethostbyname' function. */
#define HAVE_GETHOSTBYNAME 1

/* Define to 1 if you have the `gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have the `getprogname' function. */
/* #undef HAVE_GETPROGNAME */

/* Define to 1 if you have the `getrandom' function. */
//#define HAVE_GETRANDOM 1

/* Define if the GNU gettext() function is already present or preinstalled. */
//#define HAVE_GETTEXT 0

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the iconv() function and it works. */
/* #undef HAVE_ICONV */

/* Enable Linux inotify() usage */
//#define HAVE_INOTIFY 0

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Enable BSD kqueue() usage */
/* #undef HAVE_KQUEUE */

/* Support for libsystemd */
//#define HAVE_LIBSYSTEMD 0

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the `memchr' function. */
#define HAVE_MEMCHR 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the `mkdir' function. */
//#define HAVE_MKDIR 0

/* Define to 1 if you have the <ndbm.h> header file. */
/* #undef HAVE_NDBM_H */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
//#define HAVE_NETINET_IN_H 1

/* Support for Linux netlink */
//#define HAVE_NETLINK /**/

/* Support for PF_ROUTE */
/* #undef HAVE_PF_ROUTE */

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* Define to 1 if you have the `putenv' function. */
//#define HAVE_PUTENV 0

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `setegid' function. */
//#define HAVE_SETEGID 0

/* Define to 1 if you have the `seteuid' function. */
//#define HAVE_SETEUID 0

/* Define to 1 if you have the `setproctitle' function. */
/* #undef HAVE_SETPROCTITLE */

/* Define to 1 if you have the `setregid' function. */
//#define HAVE_SETREGID 0

/* Define to 1 if you have the `setresgid' function. */
//#define HAVE_SETRESGID 0

/* Define to 1 if you have the `setresuid' function. */
//#define HAVE_SETRESUID 0

/* Define to 1 if you have the `setreuid' function. */
//#define HAVE_SETREUID 0

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if `stat' has the bug that it succeeds when given the
   zero-length file name argument. */
/* #undef HAVE_STAT_EMPTY_STRING_BUG */

/* Define to 1 if stdbool.h conforms to C99. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strcspn' function. */
#define HAVE_STRCSPN 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcpy' function. */
/* #undef HAVE_STRLCPY */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the `strncasecmp' function. */
#define HAVE_STRNCASECMP 1

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if you have the `strspn' function. */
#define HAVE_STRSPN 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Support for struct ip_mreq */
/* #undef HAVE_STRUCT_IP_MREQ */

/* Support for struct ip_mreqn */
// #define HAVE_STRUCT_IP_MREQN /**/

/* Define if there is a struct lifconf. */
/* #undef HAVE_STRUCT_LIFCONF */

/* Define to 1 if you have the <syslog.h> header file. */
//#define HAVE_SYSLOG_H 0

/* Define to 1 if you have the <sys/capability.h> header file. */
/* #undef HAVE_SYS_CAPABILITY_H */

/* Support for sys/filio.h */
/* #undef HAVE_SYS_FILIO_H */

/* Define to 1 if you have the <sys/inotify.h> header file. */
//#define HAVE_SYS_INOTIFY_H 0

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/prctl.h> header file. */
//#define HAVE_SYS_PRCTL_H 0

/* Define to 1 if you have the <sys/random.h> header file. */
//#define HAVE_SYS_RANDOM_H 0

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Support for sys/sysctl.h */
#define HAVE_SYS_SYSCTL_H /**/

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the `uname' function. */
#define HAVE_UNAME 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Support for visibility hidden */
#define HAVE_VISIBILITY_HIDDEN /**/

/* Define to 1 if the system has the type `_Bool'. */
#define HAVE__BOOL 1

/* Define to 1 if `lstat' dereferences a symlink specified with a trailing
   slash. */
//#define LSTAT_FOLLOWS_SLASHED_SYMLINK 0

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "avahi"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "avahi (at) lists (dot) freedesktop (dot) org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "avahi"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "avahi 0.8"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "avahi"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.8"

/* Define to necessary symbol if this constant uses a non-standard name on
   your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* Define to the type of arg 1 for `select'. */
#define SELECT_TYPE_ARG1 int

/* Define to the type of args 2, 3 and 4 for `select'. */
#define SELECT_TYPE_ARG234 (fd_set *)

/* Define to the type of arg 5 for `select'. */
#define SELECT_TYPE_ARG5 (struct timeval *)

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Version number of package */
#define VERSION "0.8"

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef gid_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef uid_t */
