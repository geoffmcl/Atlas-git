EXTRA_DIST = buildmaps.sh

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
	Overlays.cxx Overlays.hxx

Atlas_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgserial -lsgdebug \
	-lplibsg -lplibpu -lplibfnt \
	$(opengl_LIBS) \
	-lpng -lz

Map_SOURCES = \
	Map.cxx \
	FlightTrack.hxx FlightTrack.cxx \
	MapMaker.cxx MapMaker.hxx \
	Output.cxx Output.hxx \
	OutputGL.cxx OutputGL.hxx \
	Overlays.cxx Overlays.hxx

Map_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgdebug \
	-lplibsg -lplibpu -lplibfnt \
	$(opengl_LIBS) \
	-lpng -lz

MapPS_SOURCES = \
	MapPS.cxx \
	FlightTrack.hxx FlightTrack.cxx \
	MapMaker.cxx MapMaker.hxx \
	Output.cxx Output.hxx \
	OutputPS.cxx OutputPS.hxx \
	Overlays.cxx Overlays.hxx

MapPS_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgdebug \
	-lplibsg -lplibpu \
	$(opengl_LIBS) \
	-lpng -lz

