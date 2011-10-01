SUBDIRS = data

bin_PROGRAMS = Atlas Map

if HAVE_CURL
bin_PROGRAMS += GetMap
endif

Atlas_SOURCES = \
	Notifications.cxx Notifications.hxx \
	Atlas.cxx \
	FlightTrack.hxx FlightTrack.cxx \
	Image.cxx Image.hxx \
	Overlays.cxx Overlays.hxx \
	AirportsOverlay.hxx AirportsOverlay.cxx \
	AirwaysOverlay.hxx AirwaysOverlay.cxx \
	FixesOverlay.hxx FixesOverlay.cxx \
	NavaidsOverlay.hxx NavaidsOverlay.cxx \
	FlightTracksOverlay.hxx FlightTracksOverlay.cxx \
	CrosshairsOverlay.hxx CrosshairsOverlay.cxx \
	RangeRingsOverlay.hxx RangeRingsOverlay.cxx \
	Tiles.cxx Tiles.hxx tiles.h \
	Searcher.cxx Searcher.hxx \
	Search.cxx Search.hxx \
	Preferences.cxx Preferences.hxx \
	Graphs.cxx Graphs.hxx \
	Culler.cxx Culler.hxx \
	Scenery.cxx Scenery.hxx \
	Cache.cxx Cache.hxx \
	LayoutManager.cxx LayoutManager.hxx \
	Bucket.cxx Bucket.hxx \
	Subbucket.cxx Subbucket.hxx \
	Palette.cxx Palette.hxx \
	misc.cxx misc.hxx \
	Globals.cxx Globals.hxx \
##	Geographics.cxx Geographics.hxx \
##	MPAircraft.cxx MPAircraft.hxx
	Geographics.cxx Geographics.hxx

Atlas_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgserial -lsgdebug \
	-lsgbucket -lsgstructure -lsgmath \
	 -lplibsg -lplibpuaux -lplibpu -lplibul -lplibfnt -lplibnet \
	$(opengl_LIBS) \
	-lpng -lz -ljpeg

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
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgdebug \
	-lsgbucket -lsgstructure -lsgmath \
	-lplibfnt -lplibsg \
	$(opengl_LIBS) \
	-lpng -lz -ljpeg \
	-lGLEW

GetMap_SOURCES = \
	GetMap.cxx

GetMap_LDADD = \
	-lcurl
