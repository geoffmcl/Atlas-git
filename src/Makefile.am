SUBDIRS = data

bin_PROGRAMS = Atlas Map

if HAVE_CURL
bin_PROGRAMS += GetMap
endif

Atlas_SOURCES = \
	Notifications.cxx Notifications.hxx \
	Atlas.cxx \
	GLUTWindow.cxx GLUTWindow.hxx \
	AtlasBaseWindow.cxx AtlasBaseWindow.hxx \
	AtlasWindow.cxx AtlasWindow.hxx \
	AtlasController.cxx AtlasController.hxx \
	FlightTrack.hxx FlightTrack.cxx \
	Image.cxx Image.hxx \
	NavData.cxx NavData.hxx \
	Overlays.cxx Overlays.hxx \
	AirportsOverlay.hxx AirportsOverlay.cxx \
	AirwaysOverlay.hxx AirwaysOverlay.cxx \
	NavaidsOverlay.hxx NavaidsOverlay.cxx \
	FlightTracksOverlay.hxx FlightTracksOverlay.cxx \
	CrosshairsOverlay.hxx CrosshairsOverlay.cxx \
	RangeRingsOverlay.hxx RangeRingsOverlay.cxx \
	Tiles.cxx Tiles.hxx tiles.h \
	TileMapper.cxx TileMapper.hxx \
	Searcher.cxx Searcher.hxx \
	Search.cxx Search.hxx \
	Preferences.cxx Preferences.hxx \
	Graphs.cxx Graphs.hxx \
	Culler.cxx Culler.hxx \
	Scenery.cxx Scenery.hxx \
	Background.cxx Background.hxx \
	Cache.cxx Cache.hxx \
	LayoutManager.cxx LayoutManager.hxx \
	Bucket.cxx Bucket.hxx \
	Subbucket.cxx Subbucket.hxx \
	Palette.cxx Palette.hxx \
	misc.cxx misc.hxx \
	Globals.cxx Globals.hxx \
	Geographics.cxx Geographics.hxx
##	PreferencesWindow.cxx PreferencesWindow.hxx
##	MPAircraft.cxx MPAircraft.hxx

# EYE - $(top_srcdir) is the source code directory for the package.  I
# don't know if referring to it this way is considered good coding
# practice, but it works.
AM_CPPFLAGS = -I$(top_srcdir)/slimgear

# EYE - Ideally (I guess), all of the libraries would be discovered in
# configure and automatically added to the LIBS variable.  However,
# it's not always easy to do so (eg, PLIB is C++, and autoconf doesn't
# work well with C++).  Also, we have three executables Atlas, Map,
# and GetMap, which don't use the same libraries.  So we specify some
# "manually" here.
Atlas_LDADD = \
	$(top_builddir)/slimgear/simgear/libslimgear.la \
	 -lplibsg -lplibpuaux -lplibpu -lplibfnt -lplibnet \
	$(opengl_LIBS)

Map_SOURCES = \
	Map.cxx \
	Tiles.cxx Tiles.hxx \
	TileMapper.cxx TileMapper.hxx \
	Bucket.cxx Bucket.hxx \
	Subbucket.cxx Subbucket.hxx \
	Palette.cxx Palette.hxx \
	Image.cxx Image.hxx \
	misc.cxx misc.hxx

Map_LDADD = \
	$(top_builddir)/slimgear/simgear/libslimgear.la \
	-lplibpu -lplibfnt -lplibsg \
	$(opengl_LIBS)

GetMap_SOURCES = \
	GetMap.cxx

# EYE - You'll note that when GetMap is linked, it is linked with all
# sorts of irrelevant libraries (GLEW, plibul, ...).  This is because
# they are detected and added to LIBS in configure.  Should this be
# changed?
GetMap_LDADD = \
	-lcurl
