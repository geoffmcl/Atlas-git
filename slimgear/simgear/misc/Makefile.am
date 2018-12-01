noinst_LTLIBRARIES = libsgmisc.la

libsgmisc_la_SOURCES = \
	sg_hash.cxx sg_hash.hxx \
	sg_path.cxx sg_path.hxx \
	sgstream.cxx sgstream.hxx \
	stdint.hxx \
	strutils.cxx strutils.hxx \
	zfstream.cxx zfstream.hxx

if OSX
    libsgmisc_la_SOURCES += CocoaHelpers.mm
endif

libsgmisc_la_CPPFLAGS = -I$(top_srcdir)

# We don't put this into _SOURCES because automake will try to compile
# it.
EXTRA_DIST = sha1.c
