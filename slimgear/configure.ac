#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

dnl EYE - I'm not sure if we really need 2.69, but this is what I used.
AC_PREREQ(2.69)

# Our version number is a combination of the SimGear version number
# (eg, 2016.3), an underscore, and a counter we increment for each
# revision to that version (ie, _0, _1, ...).
AC_INIT([slimgear], [2016.3_0])

dnl A sanity check - if we can't find this file, something is wrong.
AC_CONFIG_SRCDIR([simgear/compiler.h])

dnl Create a configuration head file, simgear_config.h (from
dnl simgear_config.in).
AC_CONFIG_HEADERS([simgear/simgear_config.h])

dnl Put macros (particularly ones generated for libtool) into the m4
dnl directory.
AC_CONFIG_MACRO_DIR([m4])

dnl EYE - I don't know if this is required on all systems, but on OS X
dnl I get errors about a non-POSIX archiver.
AM_PROG_AR

dnl EYE - I'm not sure if we really need 2.4.6, but this is what I used.
LT_PREREQ([2.4.6])

dnl Ask for libtool support.  Since we're creating a convenience
dnl library (one that is only used internally), we want a static library.
LT_INIT(disable-shared)

dnl "-Wall -Werror" are *not* compiler options - these tell Automake
dnl to turn on warnings and report them as errors.  The last,
dnl "foreign", tells Automake not to complain if we don't have
dnl GNU-standard files like THANKS.
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_OBJCXX

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h memory.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h sys/timeb.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_STRUCT_TIMEZONE
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_MKTIME
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRERROR_R
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([atexit clock_gettime floor ftime gethostbyname gethostname gettimeofday inet_ntoa memmove memset mkdir pow realpath rint select socket sqrt strdup strerror strstr strtoul])

AC_CONFIG_FILES([Makefile
                 simgear/Makefile
                 simgear/version.h
                 simgear/bucket/Makefile
                 simgear/debug/Makefile
                 simgear/io/Makefile
                 simgear/magvar/Makefile
                 simgear/math/Makefile
                 simgear/misc/Makefile
                 simgear/package/Makefile
                 simgear/serial/Makefile
                 simgear/structure/Makefile
                 simgear/threads/Makefile
                 simgear/timing/Makefile])

dnl Thread related checks
AC_CHECK_HEADER(pthread.h)
AC_SEARCH_LIBS(pthread_exit, [pthread c_r])
if test "x$ac_cv_header_pthread_h" = "xyes"; then
    CXXFLAGS="$CXXFLAGS -D_REENTRANT"
    CFLAGS="$CFLAGS -D_REENTRANT"

  if test "x$ac_cv_search_pthread_exit" = "x-lc_r"; then
    CXXFLAGS="-pthread $CXXFLAGS"
    CFLAGS="-pthread $CFLAGS"
  fi
fi

AM_CONDITIONAL(HAVE_THREADS, test "x$ac_cv_header_pthread_h" = "xyes")

thread_LIBS="$LIBS"
LIBS=""

AC_SUBST(thread_LIBS)

# Find out if we're running OS X (for use in a makefile).  For more
# info, see:
#
# https://stackoverflow.com/questions/38898591/how-to-check-the-os-with-automake
#
# Other values: linux* = linux, cygwin*|mingw* = windows
AC_CANONICAL_HOST
case $host_os in
    darwin*) AM_CONDITIONAL([OSX], true) ;;
    *) AM_CONDITIONAL([OSX], false) ;;
esac

AC_OUTPUT

echo ""
echo "Slimgear Configure Summary"
echo "=========================="

echo "Prefix: $prefix"
echo ""

if test "x$ac_cv_header_pthread_h" = "xyes"; then
   echo "Threads: pthread lib found."
else
   echo "Threads: no threads (pthread lib not found.)"
fi
