/*-------------------------------------------------------------------------
  Scenery.hxx

  Written by Brian Schack

  Copyright (C) 2008 - 2014 Brian Schack

  The scenery object is responsible for loading and displaying
  scenery, whether that scenery comes in the form of pre-rendered maps
  or "live" FlightGear scenery.  It will also display MEF (minimum
  elevation figures) on the maps.  It does *not* display navaids,
  airports, etc.

  In the MVC scheme of things, one might expect the scenery object to
  be a view.  However, it contains real data, namely live scenery and
  MEFs.  Other objects query the scenery object to find out, for
  example, the elevation at a certain point.  Because of this, the
  scenery can't just listen to notification of moves and zooms to
  update itself (unlike regular view objects).  It must be explicitly
  asked to move and zoom, *and* this must be done before other objects
  get notifications of moves or zooms (since they might immediately
  query the scenery object about the newly moved or zoomed scenery).

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
class AtlasWindow;

// Handles loading and unloading of a single texture (ie, map).  The
// texture doesn't know how to draw itself.

// EYE - make a base class that doesn't know about maximum elevation
// figures, then a derived class (MapTexture) that does.
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
    // A Scenery object needs to know what window to display
    // everything into (this is mostly because textures are loaded
    // asynchronously, and we need to make sure that the textures are
    // being created in the right OpenGL context).
    Scenery(AtlasWindow *aw);
    ~Scenery();

    AtlasWindow *win() { return _aw; }

    void move(const sgdMat4 modelViewMatrix, const sgdVec3 eye);
    void zoom(const sgdFrustum& frustum, double metresPerPixel);
    void setMEFs(bool elevationLabels) { _MEFs = elevationLabels; }

    void draw(bool lightingOn);

    bool live() const { return _live; }
    unsigned int level() const { return _level; }
    Culler::FrustumSearch* frustum() const { return _frustum; }

    // Tells us that the tile's status has changed.
    void update(Tile *t);

  protected:
    // Draws MEF labels on the scenery.
    void _label(bool live);

    AtlasWindow *_aw;		// Our owning window.
    bool _dirty;		// True if the eyepoint has moved or
				// we've zoomed.
    bool _MEFs;			// True if we need to draw MEFs.

    sgdVec3 _eye;		// Our eye point (used by the cache).
    double _metresPerPixel;	// Current zoom level.
    unsigned int _level;	// Current texture level to display.
    bool _live;			// True if we need to display live scenery.
    TileManager *_tm;
    // Bit <n> is set if we have a texture directory for level <n>.
    const std::bitset<TileManager::MAX_MAP_LEVEL>& _levels;

    Culler *_culler;
    Culler::FrustumSearch *_frustum;
    unsigned int _tileType;	// Type id for tiles in the culler object.

    // A SceneryTile object manages all the textures and live scenery
    // for a 1 degree by 1 degree (usually) chunk of the earth.
    std::map<Tile*, SceneryTile *>_tiles;

    // The cache is used to manage the loading of textures and buckets
    // (live scenery).
    Cache _cache;
};

#endif
