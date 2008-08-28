SUBDIRS = data

EXTRA_DIST = buildmaps.sh AtlasPalette

bin_PROGRAMS = Atlas Map MapPS

if HAVE_CURL
bin_PROGRAMS += GetMap
endif

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
	Projection.cxx Projection.hxx \
	Tile.cxx Tile.hxx \
	TileManager.cxx TileManager.hxx \
	Search.cxx Search.hxx \
	Preferences.cxx Preferences.hxx \
	Graphs.cxx Graphs.hxx

Atlas_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgserial -lsgdebug \
	-lsgbucket -lsgxml -lsgstructure \
	 -lplibsg -lplibpuaux -lplibpu -lplibul -lplibfnt -lplibnet \
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
	Scenery.cxx Scenery.hxx

Map_LDADD = \
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgserial -lsgdebug \
	-lsgbucket -lsgscreen -lsgstructure \
	-lplibsg -lplibpu -lplibul -lplibfnt -lplibnet \
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
	-lsgmagvar -lsgtiming -lsgmisc -lsgio -lsgserial -lsgdebug \
	-lplibsg -lplibnet -lsgbucket -lsgstructure \
	-lpng -lz

GetMap_SOURCES = \
	GetMap.cxx

GetMap_LDADD = \
	-lcurl
