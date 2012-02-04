/*-------------------------------------------------------------------------
  Scenery.hxx

  Written by Brian Schack

  Copyright (C) 2008 - 2012 Brian Schack

  The scenery object is responsible for loading and displaying
  scenery, whether that scenery comes in the form of pre-rendered maps
  or "live" FlightGear scenery.  It will also display MEF (minimum
  elevation figures) on the maps.  It does *not* display navaids,
  airports, etc.

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

#ifndef _SCENERY_H_
#define _SCENERY_H_

#include <bitset>
#include <map>

#if defined( __APPLE__)		// For GLubyte and GLuint
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif
#include <simgear/math/SGMath.hxx> // SGVec3
#include <simgear/misc/sg_path.hxx>

#include "Cache.hxx"		// Cache
#include "Culler.hxx"		// Culler::FrustumSearch
#include "Notifications.hxx"
#include "Tiles.hxx"		// TileManager::MAX_MAP_LEVEL

// Forward class declarations
class AtlasBaseWindow;

// Handles loading and unloading of a single texture (ie, map).  The
// texture doesn't know how to draw itself.
class Texture {
    // When name() is called and there's no texture, we substitute a
    // default 8x8 checkerboard texture.  The data is in
    // __defaultImage, and the texture name is in __defaultTexture.
    static const int __defaultSize = 8;
    static GLubyte __defaultImage[__defaultSize][__defaultSize][3];
    static GLuint __defaultTexture;

  public:
    Texture();
    ~Texture();

    // Load the given file.  Maps can have a maximum elevation
    // embedded in them as a text comment.  If you're interested in
    // the value, pass a pointer to a float.
    void load(SGPath f, float *maximumElevation = NULL);
    void unload();
    bool loaded() const { return _name != 0; }
    unsigned int size() { return _size; }

    // Texture name.
    GLuint name() const;

  protected:
    GLuint _name;		// Texture name, initialized to 0.
    unsigned int _size;		// Size, in bytes.
};

class SceneryTile;
class Scenery {
  public:
    // A Scenery object needs a TileManager to get tile information,
    // and needs to know what window to display everything into (this
    // is mostly because textures are loaded asynchronously, and we
    // need to make sure that the textures are being created in the
    // right OpenGL context).
    Scenery(TileManager *tm, AtlasBaseWindow *win);
    ~Scenery();

    AtlasBaseWindow *win() { return _win; }

    void setBackgroundImage(const SGPath& f);

    void move(const sgdMat4 modelViewMatrix, const sgdVec3 eye);
    void zoom(const sgdFrustum& frustum, double metresPerPixel);
    void setMEFs(bool elevationLabels) { _MEFs = elevationLabels; }

    void draw(bool lightingOn);

    bool live() const { return _live; }
    unsigned int level() const { return _level; }
    Culler::FrustumSearch* frustum() const { return _frustum; }

    // Calculates the intersection of the viewing ray that goes
    // through the window at <x, y> with the earth, returning true if
    // the ray intersects the earth (in which case c contains the
    // point of intersection).  If it intersects with live scenery,
    // then validElevation (if it is non-null) is set to true, and the
    // elevation in c is the actual elevation at that point.  If
    // validElevation is false, then the elevation in c is the
    // elevation with the earth ellipsoid.  x and y use window
    // coordinates, with <0.0, 0.0> at the top left corner, and <w, h>
    // at the bottom right corner (where w and h are the window width
    // and height respectively).

    // EYE - we should also return the bucket id so that we can
    // quickly get map information.
    bool intersection(double x, double y, SGVec3<double> *c, 
		      bool *validElevation = NULL);

  protected:
    void _createWorlds(bool force = false);

    void _label(bool live);

    AtlasBaseWindow *_win;	// Our owning window.
    bool _dirty;		// True if the eyepoint has moved or
				// we've zoomed.
    bool _MEFs;			// True if we need to draw MEFs.

    sgdVec3 _eye;		// Our eye point (used by the cache).
    double _metresPerPixel;	// Current zoom level.
    unsigned int _level;	// Current texture level to display.
    bool _live;			// True if we need to display live scenery.
    // Bit <n> is set if we have a texture directory for level <n>.
    const std::bitset<TileManager::MAX_MAP_LEVEL>& _levels;

    Culler *_culler;
    Culler::FrustumSearch *_frustum;
    unsigned int _tileType;	// Type id for tiles in the culler object.

    TileManager *_tm;

    // Background world texture and display list.
    Texture _world;
    GLuint _backgroundWorld;

    // A SceneryTile object manages all the textures and live scenery for a 1
    // degree by 1 degree (usually) chunk of the earth.
    std::vector<SceneryTile *>_tiles;

    // The cache is used to manage the loading of textures and buckets
    // (live scenery).
    Cache _cache;
};

#endif
