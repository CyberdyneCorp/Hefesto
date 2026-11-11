/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The Hefesto Authors
 *
 * Build configuration of the embedded Tcl 8.6.16 (cmake/Tcl.cmake;
 * openspec/changes/m4-scripting/design.md D3). It replaces the header that
 * Tcl's unix/configure would generate: Hefesto compiles Tcl's sources with
 * its own toolchain (every preset, including the sanitizer, fuzz and ipados
 * cross builds) instead of running configure, so the platform facts are
 * written down here once. Values follow unix/configure.in and unix/tcl.m4 of
 * Tcl 8.6.16 for 64-bit macOS/iOS (Darwin) and Linux (glibc). Passed with
 * -imacros, as configure's DEFS do (-DHAVE_TCL_CONFIG_H -imacros tclConfig.h).
 *
 * Deliberate differences from a configure build:
 *   - dynamic loading is compiled out (generic/tclLoadNone.c): `load` of a
 *     binary extension can never succeed (design D5, hefesto-security);
 *   - no CoreFoundation notifier on Apple platforms (the select-based Unix
 *     notifier is used everywhere), so the core links no Apple framework;
 *   - the compiled-in library and package paths are empty (Tcl.cmake), so
 *     Tcl never looks for its scripts on the host (embedded library).
 *
 * Owner: TCL (openspec/changes/m4-scripting/design.md D2).
 */

#ifndef HEFESTO_TCL_CONFIG_H
#define HEFESTO_TCL_CONFIG_H

#if !defined(__LP64__) && !defined(_LP64)
#error "the embedded Tcl configuration assumes a 64-bit (LP64) target"
#endif

/* --- Headers and functions common to Darwin and glibc ------------------ */
#define STDC_HEADERS 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_TIME_H 1
#define TIME_WITH_SYS_TIME 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_TERMIOS_H 1
#define HAVE_LANGINFO 1
#define HAVE_GETCWD 1
#define HAVE_OPENDIR 1
#define HAVE_STRTOL 1
#define HAVE_WAITPID 1
#define HAVE_MKSTEMP 1
#define HAVE_MKSTEMPS 1
#define HAVE_MKTIME 1
#define HAVE_GMTIME_R 1
#define HAVE_LOCALTIME_R 1
#define HAVE_TM_GMTOFF 1
#define HAVE_STRUCT_STAT_ST_BLOCKS 1
#define HAVE_STRUCT_STAT_ST_BLKSIZE 1
#define HAVE_BLKCNT_T 1
#define HAVE_INTPTR_T 1
#define HAVE_UINTPTR_T 1
#define HAVE_SIGNED_CHAR 1
#define HAVE_FTS 1
#define NO_UNION_WAIT 1

/* Sockets (the `socket` command is removed by the sandbox, design D5; the
 * channel code still compiles). */
#define HAVE_GETADDRINFO 1
#define HAVE_FREEADDRINFO 1
#define HAVE_GAI_STRERROR 1
#define HAVE_GETNAMEINFO 1
#define HAVE_STRUCT_ADDRINFO 1
#define HAVE_STRUCT_IN6_ADDR 1
#define HAVE_STRUCT_SOCKADDR_IN6 1
#define HAVE_STRUCT_SOCKADDR_STORAGE 1

/* Threads (design D3: threads enabled). */
#define TCL_THREADS 1
#define _REENTRANT 1
#define _THREAD_SAFE 1
#define HAVE_PTHREAD_ATTR_SETSTACKSIZE 1
#define HAVE_PTHREAD_ATFORK 1
#define HAVE_GETPWUID_R 1
#define HAVE_GETPWUID_R_5 1
#define HAVE_GETPWNAM_R 1
#define HAVE_GETPWNAM_R_5 1
#define HAVE_GETGRGID_R 1
#define HAVE_GETGRGID_R_5 1
#define HAVE_GETGRNAM_R 1
#define HAVE_GETGRNAM_R_5 1
/* The per-thread object allocator hides heap errors from the sanitizers;
 * Tcl.cmake defines PURIFY (plain malloc) in sanitizer builds instead. */
#ifndef PURIFY
#define USE_THREAD_ALLOC 1
#endif

/* Static library, 64-bit wide integers, libtommath, zlib (hefesto::zlib). */
#define STATIC_BUILD 1
#define TCL_CFG_DO64BIT 1
#define TCL_WIDE_INT_IS_LONG 1
#define MP_PREC 4
#define HAVE_ZLIB 1
#define HAVE_HIDDEN 1
#define MODULE_SCOPE extern __attribute__((__visibility__("hidden")))
#define TCL_CFGVAL_ENCODING "iso8859-1"
#define TCL_CFG_OPTIMIZED 1

#if defined(__APPLE__)
/* --- Darwin (macOS and iOS) --------------------------------------------- */
#define MAC_OSX_TCL 1
#define _DARWIN_C_SOURCE 1
#define HAVE_SYS_FILIO_H 1
#define HAVE_GETATTRLIST 1
#define HAVE_COPYFILE_H 1
#define HAVE_COPYFILE 1
#define HAVE_CHFLAGS 1
#define HAVE_AVAILABILITYMACROS_H 1
#define HAVE_WEAK_IMPORT 1
#define HAVE_MTSAFE_GETHOSTBYNAME 1
#define HAVE_MTSAFE_GETHOSTBYADDR 1
#define TCL_DEFAULT_ENCODING "utf-8"
#define TCL_WIDE_CLICKS 1
#define TCL_SHLIB_EXT ".dylib"
#elif defined(__linux__)
/* --- Linux (glibc) ------------------------------------------------------- */
#define HAVE_TIMEZONE_VAR 1
#define HAVE_GETHOSTBYNAME_R 1
#define HAVE_GETHOSTBYNAME_R_6 1
#define HAVE_DECL_GETHOSTBYNAME_R 1
#define HAVE_GETHOSTBYADDR_R 1
#define HAVE_GETHOSTBYADDR_R_8 1
#define HAVE_DECL_GETHOSTBYADDR_R 1
#define TCL_SHLIB_EXT ".so"
#else
#error "the embedded Tcl configuration covers Darwin and Linux only"
#endif

#endif /* HEFESTO_TCL_CONFIG_H */
