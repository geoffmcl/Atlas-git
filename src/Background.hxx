/*-------------------------------------------------------------------------
  Background.hxx

  Written by Brian Schack

  Copyright (C) 2012 Brian Schack

  EYE - check the introductory docs.  Also check the class layout and
  docs for members and functions.  Finally, should the status layer
  really be part of this class, or should we make it another class
  (like Scenery).

  The background object draws a basic textured or coloured earth,
  coloured optionally by a tile status texture.  It is meant to be
  drawn before scenery is drawn.  

  The background texture is loaded from a file, and can be any JPG or
  PNG image.  By default we load "background.jpg" in the standard
  Atlas directory.  The image is assumed to be an equirectangular
  projection, where the north pole is at the top, the south at the
  bottom, 180 W at the left edge, the prime meridian down the centre,
  and 180 E at the right edge.  

  If the caller chooses a coloured earth, this object gets the colour
  from the "Ocean" material of the current palette.  If it does not
  exist, it falls back to a default colour.

  If the status layer is turned on, a translucent representation of
  tile status is drawn on top - mapped tiles are green, unmapped (or
  partially mapped) but downloaded tiles are red, and undownloaded
  tiles are white.  Tiles that don't exist in the FlightGear universe
  are clear.

  When this object is created, it expects a valid OpenGL context, that
  preferences have been initialized, and that the AtlasController has
  been initialized.

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

#ifndef _BACKGROUND_H_
#define _BACKGROUND_H_

#include <vector>
#include <map>

#if defined( __APPLE__)		// For GLubyte, GLuint
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif

#include "Scenery.hxx"		// Texture
#include "Notifications.hxx"

class AtlasWindow;
class Background: public Subscriber {
  public:
    Background(AtlasWindow *aw);
    ~Background();

    // True if the background is an image, false if it's ocean
    // coloured.
    bool useImage() { return _useImage; }
    void setUseImage(bool b);

    // Calling 'setImage()' will load the file and create the texture.
    // It will *not* change 'useImage'.
    void setImage(SGPath &p);

    // Possible different states for a tile.
    enum TileStatus {NONEXISTENT, SCENERY, UNMAPPED, MAPPED, TO_BE_MAPPED,
		     MAPPING, TILETYPE_COUNT};
    // Colours the area of status texture occupied by the given tile
    // according to its status.
    void setTileStatus(Tile *tile);
    // Colours the area of status texture

    // EYE - make more parameters const?  Use that instead of
    // references all over?
    void setTileStatus(Tile *t, TileStatus);

    void draw();
    void drawOutlines();

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    // Sets up our vertex arrays (_vertices, ...)
    void _init();
    // Returns the index in _vertices corresponding to the given
    // GeoLocation or <lat, lon> pair.  The 3 vertices are at
    // _vertices[i * 3], _vertices[i * 3 + 1], and _vertices[i * 3 +
    // 2].
    GLuint _getVertexIndex(GeoLocation &loc);
    GLuint _getVertexIndex(int lat, int lon);

    // These are helper routines for creating chunk and tile
    // boundaries.  They create a single line segment between v1 and
    // v2, adding that segment to the lists for the given chunks or
    // tiles (if the chunk/tile is non-NULL).
    void _addChunkSegment(GeoLocation& v1, GeoLocation& v2,
			  Chunk *c1, Chunk *c2);
    void _addTileSegment(GeoLocation& v1, GeoLocation& v2,
			 Tile *t1, Tile *t2);

    // An array of tile colours, corresponding to the types in
    // TileStatus.
    static GLubyte __colours[TILETYPE_COUNT][4];

    // Our owning window.
    AtlasWindow *_aw;

    // True if we need to regenerate our display list
    bool _dirty;
    // Our display lists.
    GLuint _DL, _latLonDL;
    // True if we're displaying a background image, false if we're
    // using a colour
    bool _useImage;
    // True if we should show the status layer, false otherwise.
    bool _showOutlines;
    // A default clear texture substituted for the background image
    // and/or the tile status texture.
    GLuint _clearTexture;
    // The texture showing tile status.
    GLuint _statusTexture;
    // Our background image, as a texture
    Texture _image;

    // All of our vertices (3 floats for each vertex).  The first
    // triplet is the south pole, followed by all points at latitude
    // -89 (starting with longitude -180, -179, ..., 179), latitude
    // -88, ....  The last triplet is the north pole.
    std::vector<float> _vertices;

    // All chunk outlines (which kind of look like lines of latitude
    // and longitude), specified as indices to vertex pairs (ie, to be
    // used in GL_LINES mode).  For example, _latLonLines[0] and
    // _latLonLines[1] form the first pair, _latLonLines[2] and
    // _latLonLines[3] form the second pair, etc.  Each GLuint is an
    // index into _vertices, where a GLuint of i specifies 3 floats in
    // _vertices: i * 3, i * 3 + 1, and i * 3 + 2.
    std::vector<GLuint> _latLonLines;

    // Per-chunk outlines, specified as a map from a chunk's canonical
    // latitude and longitude to a vector of vertex index pairs (like
    // _latLonLines).  Each GLuint is an index into _vertices.
    std::map<GeoLocation, std::vector<GLuint> > _chunkOutlines;
    // Ditto for tiles.
    std::map<GeoLocation, std::vector<GLuint> > _tileOutlines;

    // An array of triangle strips specifying points on the earth at 1
    // degree intervals.  These index the _vertices array, and
    // correspond to the texture coordinates in _imageTexCoords and
    // _statusTexCoords (and as such, they contain 360 points for each
    // degree of longitude, even the south and north poles, and have
    // points for -180 *and* 180 degrees longitude).  There are 180
    // triangle strips in _indices, one for each degree of latitude,
    // starting at the south pole.  Each triangle strip consists of
    // 361 * 2 points, or 720 triangles.
    GLuint _indices[180 * 361 * 2];
    // These are to make the call to glMultiDrawElements happy.  The
    // _sphere array contains pointers into _indices, one for each
    // triangle strip, and _counts tells glMultiDrawElements how many
    // points are in each triangle strip (always 361 * 2).
    const GLvoid *_sphere[180];
    GLsizei _counts[180];
    // Texture coordinates for our background image and status
    // textures.
    std::vector<GLfloat> _imageTexCoords, _statusTexCoords;
};

#endif
