/* ==========================================================
    *** DO NOT MODIFY config.h ***
    Instead modify config.h.cmake, and re-run cmake configure
   ========================================================== */
#ifndef _ATLAS_CONFIG_H_
#define _ATLAS_CONFIG_H_
#ifdef _MSC_VER
/* for WINDOWS ONLY */
#include <winsock2.h> // also includes <windows.h> - must be before <GL/gl.h>
#endif // _MSC_VER

/* Define to 1 if you have the `bcopy' function. */
#cmakedefine HAVE_BCOPY 1

/* Define to 1 if you have the `drand48' function. */
#cmakedefine HAVE_DRAND48 1

/* Define to 1 if you have the `fabsl' function. */
#cmakedefine HAVE_FABSL 1

/* Define to 1 if you have the <fcntl.h> header file. */
#cmakedefine HAVE_FCNTL_H 1

/* Define to 1 if you have the `ftime' function. */
#cmakedefine HAVE_FTIME 1

/* Define to 1 if you have the `getitimer' function. */
#cmakedefine HAVE_GETITIMER 1

/* Define to 1 if you have the `GetLocalTime' function. */
#cmakedefine HAVE_GETLOCALTIME 1

/* Define to 1 if you have the <getopt.h> header file. */
#cmakedefine HAVE_GETOPT_H 1

/* Define to 1 if you have the `getrusage' function. */
#cmakedefine HAVE_GETRUSAGE 1

/* Define to 1 if you have the `gettimeofday' function. */
#cmakedefine HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the <inttypes.h> header file. */
#cmakedefine HAVE_INTTYPES_H 1

/* Define to 1 if you have the <malloc.h> header file. */
#cmakedefine HAVE_MALLOC_H 1

/* Define to 1 if you have the `memcpy' function. */
#cmakedefine HAVE_MEMCPY 1

/* Define to 1 if you have the <memory.h> header file. */
#cmakedefine HAVE_MEMORY_H 1

/* Define to 1 if you have the `mkfifo' function. */
#cmakedefine HAVE_MKFIFO 1

/* Define to 1 if you have the `mktime' function. */
#cmakedefine HAVE_MKTIME 1

/* Define to 1 if you have the `rand' function. */
#cmakedefine HAVE_RAND 1

/* Define to 1 if you have the `random' function. */
#cmakedefine HAVE_RANDOM 1

/* Define to 1 if you have the `rint' function. */
#cmakedefine HAVE_RINT 1

/* Define to 1 if you have the `setitimer' function. */
#cmakedefine HAVE_SETITIMER 1

/* Define to 1 if you have the `signal' function. */
#cmakedefine HAVE_SIGNAL 1

/* Define to 1 if you have the <stdint.h> header file. */
#cmakedefine HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#cmakedefine HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#cmakedefine HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#cmakedefine HAVE_STRING_H 1

/* Define to 1 if you have the `strstr' function. */
#cmakedefine HAVE_STRSTR 1

/* Define to 1 if you have the <sys/param.h> header file. */
#cmakedefine HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#cmakedefine HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/timeb.h> header file. */
#cmakedefine HAVE_SYS_TIMEB_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#cmakedefine HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#cmakedefine HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the `timegm' function. */
#cmakedefine HAVE_TIMEGM 1

/* Define to 1 if you have the `truncf' function. */
#cmakedefine HAVE_TRUNCF 1

/* Define to 1 if you have the <unistd.h> header file. */
#cmakedefine HAVE_UNISTD_H 1

/* Define to 1 if you have the <values.h> header file. */
#cmakedefine HAVE_VALUES_H 1

/* Define to 1 if you have the `vprintf' function. */
#cmakedefine HAVE_VPRINTF 1

/* Define to 1 if you have the <windows.h> header file. */
#cmakedefine HAVE_WINDOWS_H 1

/* Define to 1 if you have the ANSI C header files. */
#cmakedefine STDC_HEADERS 1

/* Name of package */
#ifndef PACKAGE
#define PACKAGE "atlas"
#endif

/* Define to the address where bug reports for this package should be sent. */
#ifndef PACKAGE_BUGREPORT
#define PACKAGE_BUGREPORT "https://sourceforge.net/p/atlas/feature-requests/"
#endif

/* Define to the full name of this package. */
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "Atlas"
#endif

/* Define to the full name and version of this package. */
#ifndef PACKAGE_STRING
#define PACKAGE_STRING "Atlas 0.6.0"
#endif

/* Define to the one symbol short name of this package. */
#ifndef PACKAGE_TARNAME
#define PACKAGE_TARNAME "atlas"
#endif

/* Define to the home page for this package. */
#ifndef PACKAGE_URL
#define PACKAGE_URL "undefined"
#endif

/* Define to the version of this package. */
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "0.6.0"
#endif

/* Version number of package */
#ifndef VERSION
#define VERSION "0.6.0"
#endif

// The following failed in this script
#if 0 // 000000000000000000000000000000000000000

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define as the return type of signal handlers (`int' or `void'). */
/* #undef RETSIGTYPE */

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
/* #undef TIME_WITH_SYS_TIME */

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

#endif // 000000000000000000000000000000000000000

// Original generated 2020/05/15 16:42:51 UTC, by in2cmake.pl
#endif // _ATLAS_CONFIG_H_
// eof
