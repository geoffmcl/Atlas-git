/*-------------------------------------------------------------------------
  Background.cxx

  Written by Brian Schack

  Copyright (C) 2012 Brian Schack

  This file is part of Atlas.

  Atlas is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Atlas is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU General Public License
  along with Atlas.  If not, see <http://www.gnu.org/licenses/>.
  ---------------------------------------------------------------------------*/

// This is a special exception to the usual rule of including our
// include file first.  We include gl.h in Background.hxx (for some
// type definitions) and glew insists on being loaded before OpenGL.
#include <GL/glew.h>

// Our include file
#include "Background.hxx"

// C++ system files
#include <set>

// Our libraries' include files
#include "AtlasWindow.hxx"
#include "AtlasController.hxx"
#include "Geographics.hxx"
#include "Palette.hxx"

using namespace std;

// The tile status texture.  Using non-power of 2 textures seems to be
// pretty slow (at least on my machine), so we use power of 2 sizes.
const int statusImageWidth = 512;  // The nearest power of 2 >= 360
const int statusImageHeight = 256; // The nearest power of 2 >= 180

Background::Background(AtlasWindow *aw): 
    _aw(aw), _dirty(true), _DL(0), _latLonDL(0)
{
    // EYE - always load a default background ("background.jpg") - use
    // preferences?  Add an interface to load other backgrounds?

    // EYE - Subscribe to palette and scenery changes.  Later, when we
    // do mapping, how can we find out about tiles to be rendered and
    // the tile being rendered?  Where do we look?

    // EYE - should I avoid OpenGL calls here?  Can/should we assume
    // that there's a valid context?  Maybe I should delay everything
    // until the first call to draw().  Note that _init() makes no
    // OpenGL calls.

    // Generate display list ids.
    _DL = glGenLists(1);
    assert(_DL != 0);

    // Initialize the clear texture.
    GLubyte data[1][1][4];
    data[0][0][0] = 0.0;
    data[0][0][1] = 0.0;
    data[0][0][2] = 0.0;
    data[0][0][3] = 0.0;
    glGenTextures(1, &_clearTexture);
    assert(_clearTexture > 0);
    glBindTexture(GL_TEXTURE_2D, _clearTexture);

    // Since this texture is clear, we don't care about aliasing
    // artifacts, so GL_NEAREST is good enough for minification and
    // magnification.  I'm not so sure about the best texture wrap
    // parameter.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA2, 1, 1, 0, 
		 GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Initialize the tile status texture.
    GLubyte statusImage[statusImageHeight][statusImageWidth][4];

    // Initialize the status texture to be completely clear.
    for (int lon = 0; lon < statusImageWidth; lon++) {
	for (int lat = 0; lat < statusImageHeight; lat++) {
	    statusImage[0][0][0] = (GLubyte)0;
	    statusImage[0][0][1] = (GLubyte)0;
	    statusImage[0][0][2] = (GLubyte)0;
	    statusImage[0][0][3] = (GLubyte)0;
	}
    }

    // Generate a texture.
    glGenTextures(1, &_statusTexture);
    glBindTexture(GL_TEXTURE_2D, _statusTexture);

    // EYE - GL_LINEAR instead of GL_NEAREST?  Mipmaps?  And what's
    // the best texture wrap parameter?
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Because this only displays a few different colours at a few
    // different opacities, we only allocate 2 bits per channel
    // (GL_RGBA2).
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA2, statusImageWidth, 
		 statusImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
		 statusImage);
    TileManager *tm = _aw->ac()->tileManager();
    TileIterator ti(tm, TileManager::ALL);
    for (Tile *t = ti.first(); t; t = ti++) {
	setTileStatus(t);
    }

    // Set up defaults.
    setUseImage(false);

    // Initialize our vertex arrays.
    _init();
}

Background::~Background()
{
    glDeleteLists(_DL, 1);
    glDeleteLists(_latLonDL, 1);
    glDeleteTextures(1, &_clearTexture);
    glDeleteTextures(1, &_statusTexture);
}

// EYE - we need to decide once and for all how to deal with _dirty,
// as well as what we should really subscribe to, if anything at all.
void Background::setUseImage(bool b)
{
    if (b != _useImage) {
	_useImage = b;
	_dirty = true;
    }
}

void Background::setImage(SGPath &p)
{
    _image.load(p);
    if (_useImage) {
	_dirty = true;
    }
}

// Colours the area of status texture occupied by the given tile
// according to its status.
void Background::setTileStatus(Tile *tile)
{
    TileStatus status;
    if (tile->isType(TileManager::MAPPED)) {
	status = MAPPED;
    } else if (tile->isType(TileManager::UNMAPPED)) {
	status = UNMAPPED;
    } else {
	status = SCENERY;
    }
    setTileStatus(tile, status);
}

void Background::setTileStatus(Tile *t, TileStatus status)
{
    // Set the colour of the appropriate square(s) in the status
    // texture.
    int width = t->width();
    const GeoLocation& loc = t->loc();
    // Change only that part of the status texture occupied by the
    // given tile.
    glBindTexture(GL_TEXTURE_2D, _statusTexture);
    for (int lon = loc.lon(); lon < loc.lon() + width; lon++) {
    	int lat = loc.lat();
    	// EYE - make status texture mimic arrangement of background
    	// map (lon = 0 in the middle)?
    	glTexSubImage2D(GL_TEXTURE_2D, 0, (lon + 180) % 360, lat, 1, 1, 
    			GL_RGBA, GL_UNSIGNED_BYTE, __colours[status]);
    }
}

// This colour is the default ocean colour {0.671, 0.737, 0.745, 1.0}
// as it would appear under default lighting.  If the user generates
// maps with other material or lighting parameters, then there will be
// a noticable edge where scenery tiles meet ocean.
//
// Ideally we should calculate this value based on the ocean colour of
// the palette used to generate maps, but that's difficult for 2
// reasons: (a) We don't know what palette and lighting were used to
// generate the maps, and (b) Even if we knew (a), we would probably
// need to render to a texture (or something equivalent) to find out
// how it would look in the end.  It seems like too much trouble.
// Perhaps it could be added to a preferences dialogue?
GLfloat _ocean[4] = {0.573, 0.631, 0.651, 1.0};

// EYE - figure out what needs to be put in display lists, how many we
// need, and when to recreate them.
void Background::draw()
{
    if (_dirty) {
    	glNewList(_DL, GL_COMPILE); {
    	    // We modify the polygon offset, as well as texture units
    	    // 0 and 1.
            glPushAttrib(GL_POLYGON_BIT | GL_TEXTURE_BIT);
    	    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT); {
    		// The background consists of three layers:
    		//
    		// (1) A background ocean colour (always present),
    		//     used to colour open ocean.
    		//
    		// (2) A background image (optional).  If not
    		//     displayed, use a clear texture.
    		//
    		// (3) A tile status texture.  This texture and the
    		//     previous texture are combined.

    		////////////////////////////////////////////////////////////
    		// (1) Ocean colour.
    		glColor4fv(_ocean);

    		////////////////////////////////////////////////////////////
    		// (2) Background image

    		// Configure texture unit 0.
    		glActiveTexture(GL_TEXTURE0);
    		glEnable(GL_TEXTURE_2D);
    		if (_useImage) {
    		    // This texture sits underneath everything, so
    		    // we'll use the GL_REPLACE texture function.
    		    // Since the texture is just an RGB texture (no
    		    // alpha), the default colour (layer 1) must have
    		    // an alpha of 1.0 to ensure that nothing from
    		    // behind shows through.
    		    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    		    glBindTexture(GL_TEXTURE_2D, _image.name());
    		} else {
    		    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    		    glBindTexture(GL_TEXTURE_2D, _clearTexture);
    		}

    		////////////////////////////////////////////////////////////
    		// (3) Tile status

    		// Configure texture unit 1.
    		glActiveTexture(GL_TEXTURE1);
    		glEnable(GL_TEXTURE_2D);
    		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		glBindTexture(GL_TEXTURE_2D, _statusTexture);

    		////////////////////////////////////////////////////////////
    		// Stitch the texture to the globe.  We move the
    		// background world back slightly so that the scenery,
    		// when draped over the world, is not obscured by this
    		// texture map.
    	    	glEnable(GL_POLYGON_OFFSET_FILL);
    	    	glPolygonOffset(1.0, 1.0);

		// Specify the vertices.
    	    	glEnableClientState(GL_VERTEX_ARRAY);
    	    	glVertexPointer(3, GL_FLOAT, 0, &_vertices[0]);

		// Specify the textures.
    	    	glClientActiveTexture(GL_TEXTURE0);
    	    	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    	    	glTexCoordPointer(2, GL_FLOAT, 0, &_imageTexCoords[0]);

    	    	glClientActiveTexture(GL_TEXTURE1);
    	    	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    	    	glTexCoordPointer(2, GL_FLOAT, 0, &_statusTexCoords[0]);

		// Draw.
    	    	glMultiDrawElements(GL_TRIANGLE_STRIP, _counts,
    				    GL_UNSIGNED_INT, _sphere, 180);
    	    }
    	    glPopClientAttrib();
    	    glPopAttrib();
    	}
    	glEndList();
    	_dirty = false;
    }

    glCallList(_DL);
}

void Background::drawOutlines()
{

    // Generate all chunk boundaries (AKA lines of latitude and
    // longitude).  We only need to do this once.  It is actually
    // drawn near the end of this routine.
    if (_latLonDL == 0) {
    	_latLonDL = glGenLists(1);
    	assert(_latLonDL != 0);

    	// Draw the chunk boundaries.
    	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    	glNewList(_latLonDL, GL_COMPILE); {
    	    glColor3f(0.5, 0.5, 0.5);
    	    // Tell OpenGL where the vertex data is.

    	    // EYE - use glBindBuffer?
    	    glVertexPointer(3, GL_FLOAT, 0, &_vertices[0]);
    	    glEnableClientState(GL_VERTEX_ARRAY);
    	    glDrawElements(GL_LINES, _latLonLines.size(),
    			   GL_UNSIGNED_INT, &(_latLonLines[0]));
    	}
    	glEndList();
    	glPopClientAttrib();
    }

    // Draw all the chunk boundaries (AKA lat/lon lines).
    glCallList(_latLonDL);

    // Highlight the current chunk and tile, if we have such a
    // thing.
    ScreenLocation *currentLoc = _aw->currentLocation();
    if (currentLoc->coord().valid()) {
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
	glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT); {
	    // EYE - get rid of depth test everywhere?
	    glDisable(GL_DEPTH_TEST);
	    glLineWidth(2.0);

	    // Tell OpenGL where the vertex data is and enable
	    // them.
	    glVertexPointer(3, GL_FLOAT, 0, &_vertices[0]);
	    glEnableClientState(GL_VERTEX_ARRAY);

	    // Chunk boundary
	    GeoLocation mLoc(currentLoc->lat(), currentLoc->lon(), true);
	    // EYE - compare old and new GeoLocations to see if we
	    //       need to highlight a different chunk?  Ditto
	    //       for tiles?
	    // EYE - should we highlight the boundary or interior
	    //       of chunks and tiles?
	    GeoLocation cLoc = Chunk::canonicalize(mLoc);
	    // EYE - is it really necessary to do a find, or can
	    // we just check if there's a valid chunk?
	    map<GeoLocation, vector<GLuint> >::iterator ci = 
		_chunkOutlines.find(cLoc);
	    if (ci != _chunkOutlines.end()) {
		glColor3f(0.25, 1.0, 1.0);
		vector<GLuint>& civ = ci->second;
		glDrawElements(GL_LINES, civ.size(),
			       GL_UNSIGNED_INT, &(civ[0]));
	    }

	    // Tile boundary
	    GeoLocation tLoc = Tile::canonicalize(mLoc);
	    map<GeoLocation, vector<GLuint> >::iterator ti = 
		_tileOutlines.find(tLoc);
	    if (ti != _tileOutlines.end()) {
		glColor3f(1.0, 0.25, 1.0);
		vector<GLuint>& tiv = ti->second;
		glDrawElements(GL_LINES, tiv.size(),
			       GL_UNSIGNED_INT, &(tiv[0]));
	    }
	}
	glPopAttrib();
	glPopClientAttrib();
    }
}

void Background::notification(Notification::type n)
{
    if (n == Notification::MouseMoved) {
	// EYE - moving the mouse doesn't make the background
	// textures, etc, dirty, just the highlighted chunk and tile.
	// _dirty = true;

	// EYE - we also need to deal with Moved, Zoomed,
	// CursorLocation, and Rotated events, which can all change
	// what's under the mouse.  I wonder if we need a meta-event
	// to encapsulate all of this, as AtlasWindow needs to do this
	// to update the lat, lon display.
    } else {
	assert(0);
    }
}

void Background::_init()
{
    // Create a vertex for each <lat, lon> pair at 1-degree intervals.
    // We start at the south pole, then for each degree of latitude,
    // we create 360 points for all the longitudes along that
    // latitude.  Note that we create 360 points even at the south and
    // north poles, and that we duplicate the points along the
    // international date line.  This is a bit wasteful, but it makes
    // dealing with texture coordinate arrays easier.
    //
    // We also generate the texture coordinates for the image and
    // status textures.  Note that they have the same indexing scheme
    // as _vertices; again, this is to make dealing with them easier.
    for (int lat = -90; lat <= 90; lat++) {
	for (int lon = -180; lon <= 180; lon++) {
	    AtlasCoord coord(lat, lon);
	    _vertices.push_back(coord.x());
	    _vertices.push_back(coord.y());
	    _vertices.push_back(coord.z());

	    // Note that the texture is loaded "upside-down" (the
	    // first row of the image is y = 0.0 of the texture).
	    _imageTexCoords.push_back((lon + 180) / 360.0);
	    _imageTexCoords.push_back((180 - (lat + 90)) / 180.0);

	    _statusTexCoords.push_back((lon + 180) / (float)statusImageWidth);
	    _statusTexCoords.push_back((lat + 90) / (float)statusImageHeight); 
	}
    }

    // Initialize _sphere, which is an array of triangle strips used
    // to pin the textures to the earth.  Each index in _sphere points
    // to a vertex (in _vertices), and a texture coordinate pair (in
    // _imageTexCoords and _statusTexCoords).  Note that we don't use
    // quads, as might be expected, because we can't be sure that the
    // 4 points of the quad are co-planar (the earth is a bit bumpy).
    int index = 0;
    for (int lat = 0; lat < 180; lat++) {
	// There are 2 indices (which are GLuints) at each degree of
	// longitude, and we have indices at 0 *and* 360, for a total
	// of 361 * 2.
	_sphere[lat] = &(_indices[index]);
	_counts[lat] = 361 * 2;
	// The texture is "pinned" at 1 degree intervals.
	for (int lon = 0; lon <= 360; lon++) {
	    _indices[index++] = _getVertexIndex(lat - 89, lon - 180);
	    _indices[index++] = _getVertexIndex(lat - 90, lon - 180);
	}
    }

    // Create chunk and tile outlines.  Tile outlines are easy, since
    // they are always rectangles (at least on a rectangular
    // projection).  Chunks aren't - most chunks are also rectangles,
    // but the ones at the north and south poles aren't.  To handle
    // these, we resort to the rather tricky code below.

    // These are the tiles and chunks in the row above the current
    // row.
    Chunk *chunks[360];
    Tile *tiles[360];
    
    // Initialize the first row.  This is a bit of a hack, in two
    // senses: (1) We assume that the first row is spanned by a single
    // chunk and tile.  If the scenery system ever changes (highly
    // unlikely), this assumption may be voided. (2) We could just set
    // them to NULL and start the main for loop with 'lat = 89'.
    // However, this would give us a 'line' at the north pole (ie, a
    // point), and a line segment from 89 degrees north to the north
    // pole along the prime meridian.  This isn't wrong, but doesn't
    // look good, and means the north and south poles look be
    // different.
    TileManager *tm = _aw->ac()->tileManager();
    Chunk *c = tm->chunk("e000n80");
    Tile *t = tm->tile("e000n89");
    for (int lon = -180; lon < 180; lon++) {
	chunks[lon + 180] = c;
	tiles[lon + 180] = t;
    }

    // The main loop.  We go through a row one 1x1 square at a time.
    // For each square, we look at the chunk/tile above it, and to the
    // left.  If different, we add a boundary segment above and/or to
    // the left.  Note that the westernmost square has nothing to the
    // left, so we need to handle it as a special case.
    for (int lat = 88; lat >= -90; lat--) {
	for (int lon = -180; lon < 180; lon++) {
	    GeoLocation loc(lat, lon, true);
	    c = tm->chunk(loc);
	    t = tm->tile(loc);
	    Chunk *cUp = chunks[lon + 180];
	    Tile *tUp = tiles[lon + 180];

	    GeoLocation sw(lat, lon, true);
	    GeoLocation nw(lat + 1, lon, true);
	    GeoLocation ne(lat + 1, lon + 1, true);
	    if (cUp != c) {
		// The chunk above us is different than us, so create
		// a line segment between them.
		_addChunkSegment(nw, ne, c, cUp);
	    }
	    if (tUp != t) {
		// Ditto, except for tiles.
		_addTileSegment(nw, ne, t, tUp);
	    }

	    Chunk *cLeft;
	    Tile *tLeft;
	    if (lon == -180) {
		// Our western neighbour is the easternmost square.
		// It hasn't been added to the chunks or tiles array
		// yet, so we need to explicitly retrieve it.
		GeoLocation locN(lat, 179, true);
		cLeft = tm->chunk(locN);
		tLeft = tm->tile(locN);
	    } else {
		cLeft = chunks[lon + 179];
		tLeft = tiles[lon + 179];
	    }
	    if (cLeft != c) {
		// The chunk to our west is different than us, so
		// create a line segment between them.
		_addChunkSegment(sw, nw, c, cLeft);
	    }
	    if (tLeft != t) {
		// Ditto, except for tiles.
		_addTileSegment(sw, nw, t, tLeft);
	    }

	    chunks[lon + 180] = c;
	    tiles[lon + 180] = t;
	}
    }

    // Create lines of latitude and longitude from all the chunk
    // outlines.  We can't just draw all the chunk outlines because
    // adjacent chunks share edges and would be drawn twice.  So, we
    // go through all the outlines and put the line segments into a
    // set.  At the end the set will contain all the unique line
    // segments.

    // EYE - also create a tile grid?  Turn it on and off depending on
    // zoom?
    set<pair<GLuint, GLuint> > segments;
    map<GeoLocation, vector<GLuint> >::const_iterator ci;
    for (ci = _chunkOutlines.begin(); ci != _chunkOutlines.end(); ci++) {
	const vector<GLuint>& o = ci->second;
	for (unsigned int i = 0; i < o.size(); i += 2) {
	    segments.insert(make_pair(o[i], o[i + 1]));
	}
    }
    set<pair<GLuint, GLuint> >::const_iterator si;
    for (si = segments.begin(); si != segments.end(); si++) {
	pair<GLuint, GLuint> p = *si;
	_latLonLines.push_back(p.first);
	_latLonLines.push_back(p.second);
    }
}

GLuint Background::_getVertexIndex(GeoLocation &loc)
{
    return _getVertexIndex(loc.lat(true), loc.lon(true));
}

// Returns the index, i, in the _vertices array representing the given
// <lat, lon>.  The <x, y, z> will be at _vertices[i * 3], _vertices[i
// * 3 + 1], and _vertices[i * 3 + 2].  The given latitude must be
// between -90 and 90 (inclusive), the longitude between -180 and 180
// inclusive.  
//
// Note that longitudes of -180 and 180 will return different indices,
// although the vertices at those indices will have the same values.
// Ditto for the north and south poles - you will get a different
// index for <-90, 42> and <-90, 43>, even those refer to the same
// point (the south pole).  This makes dealing with texture coordinate
// arrays easier.
GLuint Background::_getVertexIndex(int lat, int lon)
{
    assert((lat >= -90) && (lat <= 90));
    lat += 90;
    assert((lon >= -180) && (lon <= 180));
    lon += 180;

    return (lat * 361) + lon;
}

// Create a single line sgement (2 points) between the two points v1
// and v2, adding the points to the chunk outline for each of the
// given chunks.  It's possible that one of the chunks is NULL, which
// indicates that it doesn't exist - the other chunk still needs the
// line segment added to its outline though.
void Background::_addChunkSegment(GeoLocation& v1, GeoLocation& v2, 
				  Chunk *c1, Chunk *c2)
{
    GLuint i1 = _getVertexIndex(v1);
    GLuint i2 = _getVertexIndex(v2);
    if (c1) {
	vector<GLuint>& o = _chunkOutlines[c1->loc()];
	o.push_back(i1);
	o.push_back(i2);
    }
    if (c2) {
	vector<GLuint>& o = _chunkOutlines[c2->loc()];
	o.push_back(i1);
	o.push_back(i2);
    }
}

// The same as _addChunkSegment, but for tiles.
void Background::_addTileSegment(GeoLocation& v1, GeoLocation& v2, 
				  Tile *c1, Tile *c2)
{
    GLuint i1 = _getVertexIndex(v1);
    GLuint i2 = _getVertexIndex(v2);
    if (c1) {
	vector<GLuint> &o = _tileOutlines[c1->loc()];
	o.push_back(i1);
	o.push_back(i2);
    }
    if (c2) {
	vector<GLuint> &o = _tileOutlines[c2->loc()];
	o.push_back(i1);
	o.push_back(i2);
    }
}

// EYE - this has to match the order in the TileStatus enum.
GLubyte Background::__colours[TILETYPE_COUNT][4] = {
    {0, 0, 0, 0},		// NONEXISTENT - clear
    {255, 255, 255, 127},	// SCENERY - white
    {255, 0, 0, 127},		// UNMAPPED - red
    {0, 255, 0, 127},		// MAPPED - green
    {0, 0, 255, 127},		// TO_BE_MAPPED - blue
    {255, 255, 0, 127}		// MAPPING - yellow
};

