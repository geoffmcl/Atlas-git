dnl Process this file with aclocal ; automake -a ; autoconf to produce a 
dnl working configure script.
dnl
dnl $Id$

AC_INIT
AC_CONFIG_SRCDIR([src/Atlas.cxx])

dnl Initialize the automake stuff
AC_PREREQ(2.52)
AM_INIT_AUTOMAKE(Atlas, 0.3.0)
AC_CONFIG_HEADERS

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

dnl Specify the simgear location
AC_ARG_WITH(simgear, [  --with-simgear=prefix   specify the prefix path to simgear])

if test "x$with_simgear" != "x" ; then
    echo "SimGear prefix path is $with_simgear"
    SIMGEAR_DIR="$with_simgear"
else 
    echo "SimGear not specified"
    SIMGEAR_DIR=
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
EXTRA_DIRS="${SIMGEAR_DIR} ${PLIB_DIR} /usr/local/plib"

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

base_LIBS="$LIBS"

AC_CHECK_LIB(pthread, pthread_exit)

dnl check for OpenGL related libraries

if test "x$ac_cv_header_windows_h" != "xyes" ; then
    dnl Reasonable stuff for non-windoze variants ... :-)

    AC_CHECK_LIB(socket, socket)
    AC_CHECK_LIB(X11, XCreateWindow)
    AC_CHECK_LIB(Xext, XShmCreateImage)
    AC_CHECK_LIB(Xi, XGetExtensionVersion)
    AC_CHECK_LIB(ICE, IceOpenConnection)
    AC_CHECK_LIB(SM, SmcOpenConnection)
    AC_CHECK_LIB(Xt, XtMalloc)
    AC_CHECK_LIB(Xmu, XmuLookupStandardColormap)

    AC_CHECK_LIB(GLcore, glNewList)
    if test "x$ac_cv_lib_GLcore_glNewList" = "xno" ; then
	dnl if no GLcore, check for GL
	AC_CHECK_LIB(GL, glNewList)
	if test "x$ac_cv_lib_GL_glNewList" = "xno" ; then
	    dnl if no GL, check for MesaGL
	    AC_CHECK_LIB(MesaGL, glNewList)
	fi
    else
	dnl if GLcore found, then also check for GL
	AC_CHECK_LIB(GL, glXCreateContext)
    fi

    dnl if using mesa, check for xmesa.h
    if test "x$ac_cv_lib_MesaGL_glNewList" = "xyes" ; then
	AC_CHECK_HEADER(GL/xmesa.h)
	if test "x$ac_cv_header_GL_xmesa_h" = "xyes"; then
            AC_DEFINE([XMESA], 1, [Define for Mesa FX mode])
            AC_DEFINE([FX], 1, [Define for Mesa FX mode])
        fi
    fi

    AC_CHECK_LIB(GLU, gluLookAt)
    if test "x$ac_cv_lib_GLU_gluLookAt" = "xno" ; then
	dnl if no GLU, check for MesaGLU
	AC_CHECK_LIB(MesaGLU, gluLookAt)
    fi

    dnl check for glut
    AC_CHECK_LIB(glut, glutGetModifiers)

    dnl test for glutGameModeString, but avoid adding glut a second time into
    dnl the list of libraries
    save_LIBS="$LIBS"
    AC_CHECK_LIB(glut, glutGameModeString)
    LIBS="$save_LIBS"
else
    dnl Win32 is a little wierd because it has to try to handle the various
    dnl winbloze-isms.  We'll just do this manually for now.

    echo Win32 specific hacks...
    AC_DEFINE([WIN32], 1, [Define if building on a Win32 platform])

    dnl force a failed check since we will be building under windoze
    # AM_CONDITIONAL(ENABLE_XMESA_FX, test "no" = "yes")

    dnl just define these to true and hope for the best
    ac_cv_lib_glut_glutGetModifiers="yes"
    ac_cv_lib_glut_glutGameModeString="yes"

    if test "x$with_sgi_opengl" = "xyes" ; then
        echo "Building with glut.dll, glu.dll, and opengl.dll"
	WIN32_GLUT=glut
	WIN32_GLU=glu
	WIN32_OPENGL=opengl
    else 
	echo "Building with glut32.dll, glu32.dll, and opengl32.dll"
	WIN32_GLUT=glut32
	WIN32_GLU=glu32
	WIN32_OPENGL=opengl32
    fi

    LIBS="$LIBS -l${WIN32_GLUT} -l${WIN32_GLU} -l${WIN32_OPENGL}"
    LIBS="$LIBS -luser32 -lgdi32"
    echo "Will link apps with $LIBS"
fi

if test "x$ac_cv_lib_glut_glutGetModifiers" = "xno"; then
    echo 
    echo "Unable to find the necessary OpenGL or GLUT libraries."
    echo "See config.log for automated test details and results ..."
    exit 1
fi

opengl_LIBS="$LIBS"
LIBS="$base_LIBS"

AC_SUBST(base_LIBS)
AC_SUBST(opengl_LIBS)

AM_CONDITIONAL(HAVE_XWINDOWS, test "x$ac_cv_lib_X11_XCreateWindow" = "xyes" )

AC_LANG_PUSH(C++)

dnl Check for "plib" without which we cannot go on
AC_CHECK_HEADER(plib/pu.h)
if test "x$ac_cv_header_plib_pu_h" != "xyes"; then
    echo
    echo "You *must* have the plib library installed on your system to build"
    echo "the FGFS simulator!"
    echo
    echo "Please see README.plib for more details."
    echo
    echo "configure aborted."
    exit
fi
AC_CHECK_LIB(plibul, ulInit,,,)

AC_LANG_POP(C++)

# Check for the presence of SimGear
AC_CHECK_HEADER(simgear/version.h)
if test "x$ac_cv_header_simgear_version_h" != "xyes"; then
    echo
    echo "You *must* have the SimGear support library installed on your system"
    echo "to build the FGFS simulator!"
    echo
    echo "Please see README.SimGear for more details."
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

#define MIN_MAJOR 0
#define MIN_MINOR 0
#define MIN_MICRO 13

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
AC_CHECK_FUNCS( ftime gettimeofday timegm memcpy bcopy mktime strstr rand \
	random setitimer getitimer signal GetLocalTime rint getrusage )

AM_CONFIG_HEADER(src/config.h)

AC_CONFIG_FILES([ \
	Makefile \
	src/Makefile \
	src/buildmaps.sh \
	src/data/Makefile
])
AC_OUTPUT

echo ""
echo "Configure Summary"
echo "================="

echo "Prefix: $prefix"
echo ""
