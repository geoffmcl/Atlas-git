dnl Process this file with autoreconf --install to produce a working
dnl configure script.
dnl
dnl $Id$

dnl I don't know if 2.69 is really needed, but it's what I have.
AC_PREREQ(2.69)

AC_INIT([Atlas], [0.6.0])

dnl A sanity check - if we can't find this file, something is wrong.
AC_CONFIG_SRCDIR([src/Atlas.cxx])

dnl Create a configuration file from src/config.h.in.
AC_CONFIG_HEADERS(src/config.h)

dnl Put macros (especially ones generated for libtool) into the m4 directory.
AC_CONFIG_MACRO_DIR([m4])

dnl Ask for libtool support.  Again, I don't know if 2.4.6 is strictly
dnl necessary; it's just what I have.
LT_PREREQ([2.4.6])
LT_INIT

dnl "-Wall -Werror" are *not* compiler options - these tell Automake
dnl to turn on warnings and report them as errors.  The last,
dnl "foreign", tells Automake not to complain if we don't have
dnl GNU-standard files like THANKS.
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

dnl ------------------------------------------------------------
dnl Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Check for MS Windows environment
AC_CHECK_HEADERS([windows.h winbase.h])

dnl Specify the plib location
AC_ARG_WITH(plib, [  --with-plib=prefix      specify the prefix path to plib])

if test "x$with_plib" != "x" ; then
    AC_MSG_NOTICE(Plib prefix path is $with_plib)
    PLIB_DIR="$with_plib"
else
    AC_MSG_NOTICE(Plib not explicitly specified)
    PLIB_DIR=
fi

dnl Specify the libcurl location
AC_ARG_WITH(libcurl, [  --with-libcurl=prefix      specify the prefix path to libcurl])

if test "x$with_libcurl" != "x" ; then
    AC_MSG_NOTICE(libcurl prefix path is $with_libcurl)
    LIBCURL_DIR="$with_libcurl"
else
    AC_MSG_NOTICE(libcurl not explicitly specified)
    LIBCURL_DIR=
fi

dnl Specify the base package location
AC_ARG_WITH(fgbase, [  --with-fgbase=prefix    specify the location of the FlightGear base package])

if test "x$with_fgbase" != "x" ; then
    AC_MSG_NOTICE(FlightGear base package location is $with_fgbase)
    fgbase_dir="$with_fgbase"
else
    AC_MSG_NOTICE(FlightGear base package location not explicitly specified)
    if test "x$prefix" != "x" ; then
	AC_MSG_NOTICE(Default ${prefix}/lib/FlightGear)
        fgbase_dir="${prefix}/lib/FlightGear"
    else
	AC_MSG_NOTICE(Default /usr/local/lib/FlightGear)
        fgbase_dir="/usr/local/lib/FlightGear"
    fi
fi
CXXFLAGS="$CXXFLAGS -DNO_OPENSCENEGRAPH_INTERFACE -DFGBASE_DIR='\"$fgbase_dir\"'"

AC_SUBST(fgbase_dir)

dnl extra library and include directories
EXTRA_DIRS="${PLIB_DIR} ${LIBCURL_DIR}"

dnl Using AM_CONDITIONAL is a step out of the protected little
dnl automake fold so it is potentially dangerous.  But, we are
dnl beginning to run into cases where the standard checks are not
dnl enough.  AM_CONDITIONALS are then referenced to conditionally
dnl build a Makefile.in from a Makefile.am which lets us define custom
dnl includes, compile alternative source files, etc.

dnl Enable serial support on Unix type systems
dnl EYE - is this useful?
AM_CONDITIONAL(ENABLE_UNIX_SERIAL, true)

dnl set the $host variable based on local machine/os
AC_CANONICAL_HOST

dnl X11 might be installed on Mac OS X or cygwin/mingwin, we don't want
dnl to use it if it is.
case "${host}" in
*-apple-darwin* | *-*-cygwin* | *-*-mingw32*)
    AC_MSG_NOTICE(no fancy X11 check)
    ;;

*)
    AC_PATH_XTRA
    ;;

esac

dnl ------------------------------------------------------------
dnl Checks for libraries.  

dnl Note that calls to AC_CHECK_LIB, if successful, add the library (-lm
dnl in this case) to the LIBS macro.
AC_CHECK_LIB(m, cos, , [AC_MSG_FAILURE([libm not found])])

base_LIBS="$LIBS"

AC_CHECK_LIB(pthread, pthread_exit, , [AC_MSG_FAILURE([libpthread not found])])

dnl check for OpenGL related libraries

case "${host}" in
*-*-cygwin* | *-*-mingw32*)
    dnl CygWin under Windoze.

    AC_MSG_NOTICE(Win32 specific hacks...)
    AC_DEFINE([WIN32], 1, [Define for Win32 platforms])
    AC_DEFINE([NOMINMAX], 1, [Define for Win32 platforms])

    LIBS="$LIBS -lglut32 -lglu32 -lopengl32 -luser32 -lgdi32"

    dnl add -lwsock32 for mingwin
    case "${host}" in
    *-*-mingw32*)
        AC_MSG_NOTICE(********** BUILDING FOR MINGW32 *************)
        network_LIBS="$network_LIBS -lws2_32"
        ;;
    esac

    AC_MSG_NOTICE(Will link apps with $LIBS)
    ;;


*-apple-darwin*)
    dnl Mac OS X

    dnl EYE - these aren't just OpenGL libraries!
    LIBS="$LIBS -framework GLUT -framework OpenGL -framework AGL -framework Carbon -framework Cocoa -lobjc"
    ;;

*)
    dnl X-Windows based machines

    dnl Skip X11 library tests if requested.
    if test "x$no_x" != "xyes"; then
        AC_SEARCH_LIBS(XCreateWindow, X11)
        AC_SEARCH_LIBS(XShmCreateImage, Xext)
        AC_SEARCH_LIBS(XGetExtensionVersion, Xi)
        AC_SEARCH_LIBS(IceOpenConnection, ICE)
        AC_SEARCH_LIBS(SmcOpenConnection, SM)
        AC_SEARCH_LIBS(XtMalloc, Xt)
        AC_SEARCH_LIBS(XmuLookupStandardColormap, Xmu)
    fi

    AC_SEARCH_LIBS(glNewList, [ GL GLcore MesaGL ])
    if test "x$ac_cv_search_glNewList" = "x-lGLcore"; then
        dnl if GLcore found, then also check for GL
        AC_SEARCH_LIBS(glXCreateContext, GL)
    fi

    dnl if using mesa, check for xmesa.h
    if test "x$ac_cv_search_glNewList" = "x-lMesaGL"; then
        AC_CHECK_HEADERS([GL/fxmesa.h])
        if test "x$ac_cv_header_GL_fxmesa_h" = "xyes"; then
            AC_DEFINE([XMESA], 1, [Define for fxmesa])
            AC_DEFINE([FX], 1, [Define for fxmesa])
        fi
    fi

    AC_SEARCH_LIBS(gluLookAt, [ GLU MesaGLU ])
    AC_SEARCH_LIBS(glutGetModifiers, [ glut freeglut ])
    ;;

esac

opengl_LIBS="$LIBS"
LIBS="$base_LIBS"

AC_SUBST(base_LIBS)
AC_SUBST(opengl_LIBS)

dnl EYE - is this needed?
AM_CONDITIONAL(HAVE_XWINDOWS, test "x$ac_cv_lib_X11_XCreateWindow" = "xyes" )

AC_CHECK_HEADERS([curl/curl.h])
AC_CHECK_LIB(curl, curl_easy_init,,,)
dnl EYE - is this needed?
AM_CONDITIONAL(HAVE_CURL, test "x$ac_cv_lib_curl_curl_easy_init" = "xyes" )

AC_LANG_PUSH(C++)

dnl Check for "plib" without which we cannot go on
AC_CHECK_HEADERS([plib/pu.h], , AC_MSG_FAILURE(plib header not found.  You *must* have the plib library installed on your system to build Atlas!))

dnl EYE - we should really check for all the plib libraries we need.  However,
dnl plib is C++, and autoconf macros don't deal well with C++ (in fact, the
dnl check below for ulInit is the only function in plib that is C-compatible).
dnl So, we just check for one library, and (1) hope that the rest exist, and 
dnl (2) put the remaining needed libraries in the relevant _LDADD variable in 
dnl the relevant Makefile.am.
AC_CHECK_LIB(plibul, ulInit, , [AC_MSG_FAILURE([plib library not found  You *must* have the plib library installed on your system to build Atlas!])])

AC_LANG_POP(C++)

dnl Check for GLEW
AC_CHECK_HEADERS([GL/glew.h], , AC_MSG_FAILURE(glew header not found.  You *must* have the glew library installed on your system to build Atlas!))
AC_CHECK_LIB(GLEW, glewInit, , [AC_MSG_FAILURE([libglew not found])])

AC_CHECK_LIB([jpeg], [jpeg_read_scanlines], , 
             [AC_MSG_FAILURE([libjpeg not found])])
AC_CHECK_LIB([png], [png_read_image], , [AC_MSG_FAILURE([libpng not found])])
AC_CHECK_LIB([z], [zlibVersion], , [AC_MSG_FAILURE([libz not found])])

dnl ------------------------------------------------------------
dnl Checks for header files.
dnl EYE - should we fail if we can't find these?
dnl EYE - put previous checks for header files in this section?
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h unistd.h values.h])

dnl ------------------------------------------------------------
dnl Checks for typedefs, structures, and compiler characteristics.
dnl EYE - should we fail if we can't find these?
AC_CHECK_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T

dnl ------------------------------------------------------------
dnl Checks for library functions.
dnl EYE - should we fail if we can't find these?
dnl EYE - put previous checks for libraries in this section?
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([atexit ceil fabs floor fmod memset modf pow realpath rint rmdir sqrt strcasecmp strchr strdup strncasecmp strrchr  strspn strcspn tolower vsnprintf])

dnl Check for optreset variable.
AC_CACHE_CHECK([for optreset], atlas_cv_var_optreset, [
  AC_TRY_LINK([#include <unistd.h>],[extern int optreset; optreset = 1;],
  [atlas_cv_var_optreset=yes],[atlas_cv_var_optreset=no])])
if test "x$atlas_cv_var_optreset" = "xyes"; then
  AC_DEFINE([HAVE_OPTRESET],[],[Defined if the optreset variable exists.])
fi

dnl Create our makefiles.
AC_CONFIG_FILES([Makefile 
	         src/Makefile
		 src/data/Makefile
		 src/data/Fonts/Makefile
		 src/data/Palettes/Makefile])

dnl Now configure slimgear.
AC_CONFIG_SUBDIRS([slimgear])

AC_OUTPUT

dnl I suppose we could us AC_MSG_NOTICE, but this looks nicer.
echo ""
echo "Atlas Configure Summary"
echo "======================="

echo "Prefix: $prefix"
echo ""
