dnl Process this file with aclocal ; automake -a ; autoconf to produce a
dnl working configure script.
dnl
dnl $Id$

AC_INIT([Atlas], [0.4.9])
AC_CONFIG_SRCDIR([src/Atlas.cxx])

dnl Initialize the automake stuff
AC_PREREQ(2.52)
AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Check for MS Windows environment
AC_CHECK_HEADER(windows.h)

dnl Specify the plib location
AC_ARG_WITH(plib, [  --with-plib=prefix      specify the prefix path to plib])

if test "x$with_plib" != "x" ; then
    echo "Plib prefix path is $with_plib"
	PLIB_DIR="$with_plib"
else
    echo "Plib not specified"
	PLIB_DIR=
fi

dnl Specify the libcurl location
AC_ARG_WITH(libcurl, [  --with-libcurl=prefix      specify the prefix path to libcurl])

if test "x$with_libcurl" != "x" ; then
    echo "libcurl prefix path is $with_libcurl"
	LIBCURL_DIR="$with_libcurl"
else
    echo "libcurl not specified"
	LIBCURL_DIR=
fi

dnl Specify the simgear location
AC_ARG_WITH(simgear, [  --with-simgear=prefix   specify the prefix path to simgear])

if test "x$with_simgear" != "x" ; then
    echo "SimGear prefix path is $with_simgear"
    SIMGEAR_DIR="$with_simgear"
else
    echo "SimGear not specified"
    SIMGEAR_DIR=
fi

dnl Specify the openscenegraph location
AC_ARG_WITH(osg, [  --with-osg=prefix   specify the prefix path to openscenegraph])

if test "x$with_osg" != "x" ; then
    echo "OpenSceneGraph prefix path is $with_osg"
    OSG_DIR="$with_osg"
else
    echo "OpenSceneGraph not specified"
    OSG_DIR=
fi

dnl Specify the base package location
AC_ARG_WITH(fgbase, [  --with-fgbase=prefix    specify the location of the FlightGear base package])

if test "x$with_fgbase" != "x" ; then
    echo "FlightGear base package location is $with_fgbase"
    fgbase_dir="$with_fgbase"
else
    echo "FlightGear base package location not specified"
    if test "x$prefix" != "x" ; then
	echo "Default ${prefix}/lib/FlightGear"
        fgbase_dir="${prefix}/lib/FlightGear"
    else
	echo "Default /usr/local/lib/FlightGear"
        fgbase_dir="/usr/local/lib/FlightGear"
    fi
fi
CXXFLAGS="$CXXFLAGS -DFGBASE_DIR='\"$fgbase_dir\"'"

AC_SUBST(fgbase_dir)

dnl set the $host variable based on local machine/os
AC_CANONICAL_HOST

case "${host}" in
*-*-irix*)
    if test "$CXX" = "CC"; then
        AR="CC -ar"
        ARFLAGS="-o"
        CXXFLAGS="$CXXFLAGS -I$with_simgear/include/simgear/compatibility"
    else
        AR="ar"
        ARFLAGS="cru"
    fi
    ;;
*)
    AR="ar"
    ARFLAGS="cru"
    ;;
esac
AC_SUBST(AR)
AC_SUBST(ARFLAGS)

dnl extra library and include directories
EXTRA_DIRS="${SIMGEAR_DIR} ${PLIB_DIR} ${LIBCURL_DIR} ${OSG_DIR} /usr/local/plib"

dnl Determine an extra directories to add to include/lib search paths
case "${host}" in
*-apple-darwin* | *-*-mingw32*)
    echo no EXTRA_DIRS for $host
    ;;

*-*-cygwin*)
    if test -d /usr/local ; then
        EXTRA_DIRS="${EXTRA_DIRS} /usr/local"
    fi
    ;;

*)
    if test -d /usr/X11R6 ; then
        EXTRA_DIR1="/usr/X11R6"
    fi
    if test -d /opt/X11R6 ; then
        EXTRA_DIR2="/opt/X11R6"
    fi
    EXTRA_DIRS="${EXTRA_DIRS} $EXTRA_DIR1 $EXTRA_DIR2 /usr/local/"
    ;;

esac
wi_EXTRA_DIRS(no, ${EXTRA_DIRS})

dnl Using AM_CONDITIONAL is a step out of the protected little
dnl automake fold so it is potentially dangerous.  But, we are
dnl beginning to run into cases where the standard checks are not
dnl enough.  AM_CONDITIONALS are then referenced to conditionally
dnl build a Makefile.in from a Makefile.am which lets us define custom
dnl includes, compile alternative source files, etc.

dnl Enable serial support on Unix type systems
AM_CONDITIONAL(ENABLE_UNIX_SERIAL, true)

AC_ARG_ENABLE(simgear-shared, [  --enable-simgear-shared    Use a shared SimGear library instead of many static libraries.],
ENABLE_SIMGEAR_SHARED=yes,ENABLE_SIMGEAR_SHARED=no)

if test "x$ENABLE_SIMGEAR_SHARED" != "xno"; then
    dnl Use shared library libSimGearCore. libSimGearScene is not required.
    simgear_LIBS="-lSimGearCore"
else
    dnl Use many static simgear libraries.
    simgear_LIBS="-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgserial -lsgdebug -lsgbucket -lsgstructure -lsgmath -lsgthreads"
fi
AC_SUBST(simgear_LIBS)

dnl X11 might be installed on Mac OS X or cygwin/mingwin, we don't want
dnl to use it if it is.
case "${host}" in
*-apple-darwin* | *-*-cygwin* | *-*-mingw32*)
    echo no fancy X11 check
    ;;

*)
    AC_PATH_XTRA
    ;;

esac

dnl Checks for libraries.

null_LIBS="$LIBS"

AC_CHECK_LIB(m, cos)
AC_SEARCH_LIBS(clock_gettime, rt)

base_LIBS="$LIBS"

AC_CHECK_LIB(pthread, pthread_exit)

dnl check for OpenGL related libraries

case "${host}" in
*-*-cygwin* | *-*-mingw32*)
    dnl CygWin under Windoze.

    echo Win32 specific hacks...
    AC_DEFINE([WIN32], 1, [Define for Win32 platforms])
    AC_DEFINE([NOMINMAX], 1, [Define for Win32 platforms])

    LIBS="$LIBS -lglut32 -lglu32 -lopengl32 -luser32 -lgdi32"

    dnl add -lwsock32 for mingwin
    case "${host}" in
    *-*-mingw32*)
        echo "********** BUILDING FOR MINGW32 *************"
        network_LIBS="$network_LIBS -lws2_32"
        ;;
    esac

    echo "Will link apps with $LIBS"
    ;;


*-apple-darwin*)
    dnl Mac OS X

    LIBS="$LIBS -framework GLUT -framework OpenGL -framework AGL -framework Carbon -lobjc"
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
        AC_CHECK_HEADER(GL/fxmesa.h)
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

AM_CONDITIONAL(HAVE_XWINDOWS, test "x$ac_cv_lib_X11_XCreateWindow" = "xyes" )

AC_CHECK_HEADER(curl/curl.h)
AC_CHECK_LIB(curl, curl_easy_init,,,)
AM_CONDITIONAL(HAVE_CURL, test "x$ac_cv_lib_curl_curl_easy_init" = "xyes" )

# Check for the presence of SimGear
AC_CHECK_HEADER(simgear/version.h)
if test "x$ac_cv_header_simgear_version_h" != "xyes"; then
    echo
    echo "You *must* have the SimGear support library installed on your system"
    echo "to build Atlas!"
    echo
    echo "configure aborted."
    exit
fi

AC_MSG_CHECKING(for proper simgear version)

AC_TRY_RUN([
#include <stdio.h>
#include <simgear/version.h>

#if !defined(SIMGEAR_VERSION)
#error simgear version too old, please upgrade.
#endif

#define STRINGIFY(X) XSTRINGIFY(X)
#define XSTRINGIFY(X) #X

#define MIN_MAJOR 2
#define MIN_MINOR 4
#define MIN_MICRO 0

int main() {
    int major, minor, micro;

    printf("%d.%d.%d or greater... ", MIN_MAJOR, MIN_MINOR, MIN_MICRO);

    sscanf( STRINGIFY(SIMGEAR_VERSION), "%d.%d.%d", &major, &minor, &micro );

    if ( major < MIN_MAJOR ) {
	 return -1;
    } else if ( major == MIN_MAJOR && minor < MIN_MINOR ) {
	 return -1;
    } else if ( major == MIN_MAJOR && minor == MIN_MINOR && micro < MIN_MICRO ){
	 return -1;
    }

    return 0;
}

],
  AC_MSG_RESULT(yes),
  [AC_MSG_RESULT(wrong version);
   AC_MSG_ERROR([Install latest simgear first...])],
  AC_MSG_RESULT(yes)
)

AC_LANG_PUSH(C++)

dnl Check for "plib" without which we cannot go on
AC_CHECK_HEADER(plib/pu.h)
if test "x$ac_cv_header_plib_pu_h" != "xyes"; then
    echo
    echo "You *must* have the plib library installed on your system to build"
    echo "Atlas!"
    echo
    echo "configure aborted."
    exit
fi
AC_CHECK_LIB(plibul, ulInit,,,)

AC_LANG_POP(C++)

dnl Check for GLEW
AC_CHECK_HEADER(GL/glew.h)
if test "x$ac_cv_header_GL_glew_h" != "xyes"; then
    echo
    echo "You *must* have the GLEW library installed on your system to build"
    echo "Atlas!"
    echo
    echo "configure aborted."
    exit
fi
AC_CHECK_LIB(GLEW, glewInit)

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS( fcntl.h unistd.h windows.h winbase.h values.h )

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS( strstr strspn strcspn tolower vsnprintf \
		sqrt rint fabs fmod floor ceil )

dnl Check for optreset variable.
AC_CACHE_CHECK([for optreset], atlas_cv_var_optreset, [
  AC_TRY_LINK([#include <unistd.h>],[extern int optreset; optreset = 1;],
  [atlas_cv_var_optreset=yes],[atlas_cv_var_optreset=no])])
if test "x$atlas_cv_var_optreset" = "xyes"; then
  AC_DEFINE([HAVE_OPTRESET],[],[Defined if the optreset variable exists.])
fi


AM_CONFIG_HEADER(src/config.h)

AC_CONFIG_FILES([ \
	Makefile \
	src/Makefile \
	src/data/Makefile \
	src/data/Fonts/Makefile \
	src/data/Palettes/Makefile
])
AC_OUTPUT

echo ""
echo "Configure Summary"
echo "================="

echo "Prefix: $prefix"
echo ""
