SUBDIRS = data

EXTRA_DIST = buildmaps.sh AtlasPalette

bin_PROGRAMS = Atlas Map MapPS

bin_SCRIPTS = buildmaps.sh

Atlas_SOURCES = \
	Atlas.cxx \
	FlightTrack.hxx FlightTrack.cxx \
	Geodesy.hxx \
	LoadPng.cxx LoadPng.hxx \
	LoadJpg.cxx LoadJpg.hxx \
	MapBrowser.cxx MapBrowser.hxx \
	Output.cxx Output.hxx \
	OutputGL.cxx OutputGL.hxx \
	Overlays.cxx Overlays.hxx \
	Projection.cxx Projection.hxx

Atlas_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgserial -lsgdebug -lsgbucket\
	-lsgxml -lplibsg -lplibpu -lplibul -lplibfnt -lplibnet \
	$(opengl_LIBS) \
	-lpng -lz -lpthread -ljpeg

Map_SOURCES = \
	Map.cxx \
	FlightTrack.hxx FlightTrack.cxx \
	MapMaker.cxx MapMaker.hxx \
	Output.cxx Output.hxx \
	OutputGL.cxx OutputGL.hxx \
	Overlays.cxx Overlays.hxx \
	Projection.cxx Projection.hxx \
	RenderTexture.cpp RenderTexture.h \
	Scenery.cxx Scenery.hxx \
	extensions.cxx extensions.hxx \
	fg_mkdir.cxx fg_mkdir.hxx

Map_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgdebug \
	-lplibsg -lplibpu -lplibul -lplibfnt -lsgbucket \
	$(opengl_LIBS) \
	-lpng -lz -ljpeg

MapPS_SOURCES = \
	MapPS.cxx \
	FlightTrack.hxx FlightTrack.cxx \
	MapMaker.cxx MapMaker.hxx \
	Output.cxx Output.hxx \
	OutputPS.cxx OutputPS.hxx \
	Overlays.cxx Overlays.hxx \
	Projection.cxx Projection.hxx \
	Scenery.cxx Scenery.hxx

MapPS_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgdebug \
	-lplibsg -lplibpu -lplibul -lsgbucket \
	$(opengl_LIBS) \
	-lpng -lz
