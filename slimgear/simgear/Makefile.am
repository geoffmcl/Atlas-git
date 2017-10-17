# This could probably be put below in _SOURCES, but it seems to fit
# better in this category.  I think either place would have the same
# effect on compilation and distribution.
EXTRA_DIST = version.h.in

SUBDIRS = \
	bucket \
	debug \
	io \
	magvar \
	math \
	misc \
	package \
	serial \
	structure \
	threads \
	timing

# We use noinst_ because we don't want to install this, just use it to
# compile Atlas.
noinst_LTLIBRARIES = libslimgear.la

# These aren't sources in the traditional sense, but we want automake
# to know about them and place them in any distribution that's
# created.
libslimgear_la_SOURCES = compiler.h constants.h sg_inlines.h version.h
# The slimgear library is composed of many sublibraries.
libslimgear_la_LIBADD = \
	bucket/libsgbucket.la \
	debug/libsgdebug.la \
	io/libsgio.la \
	magvar/libsgmagvar.la \
	math/libsgmath.la \
	misc/libsgmisc.la \
	package/libsgpackage.la \
	serial/libsgserial.la \
	structure/libsgstructure.la \
	threads/libsgthreads.la \
	timing/libsgtiming.la
