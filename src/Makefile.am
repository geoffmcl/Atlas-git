SUBDIRS = data

EXTRA_DIST = buildmaps.sh AtlasPalette

bin_PROGRAMS = Atlas Map MapPS

bin_SCRIPTS = buildmaps.sh

Atlas_SOURCES = \
	Atlas.cxx \
	FlightTrack.hxx FlightTrack.cxx \
	Geodesy.hxx \
	LoadPng.cxx LoadPng.hxx \
	MapBrowser.cxx MapBrowser.hxx \
	MapMaker.cxx MapMaker.hxx \
	Output.cxx Output.hxx \
	OutputGL.cxx OutputGL.hxx \
	Overlays.cxx Overlays.hxx \
	Projection.cxx Projection.hxx

Atlas_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgserial -lsgdebug -lsgbucket\
	-lplibsg -lplibpu -lplibul -lplibfnt -lplibnet \
	$(opengl_LIBS) \
	-lpng -lz -lpthread

Map_SOURCES = \
	Map.cxx \
	FlightTrack.hxx FlightTrack.cxx \
	MapMaker.cxx MapMaker.hxx \
	Output.cxx Output.hxx \
	OutputGL.cxx OutputGL.hxx \
	Overlays.cxx Overlays.hxx \
	Projection.cxx Projection.hxx

Map_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgdebug \
	-lplibsg -lplibpu -lplibul -lplibfnt -lsgbucket \
	$(opengl_LIBS) \
	-lpng -lz

MapPS_SOURCES = \
	MapPS.cxx \
	FlightTrack.hxx FlightTrack.cxx \
	MapMaker.cxx MapMaker.hxx \
	Output.cxx Output.hxx \
	OutputPS.cxx OutputPS.hxx \
	Overlays.cxx Overlays.hxx \
	Projection.cxx Projection.hxx

MapPS_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgdebug \
	-lplibsg -lplibpu -lplibul -lsgbucket \
	$(opengl_LIBS) \
	-lpng -lz

CPPFLAGS=-DFGBASE_DIR=\"$(fgbase_dir)\"
