/*-------------------------------------------------------------------------
  TileMapper.hxx

  Written by Brian Schack, started March 2009.

  Copyright (C) 2009, 2011 Brian Schack

  This class will render maps for a tile and write them to a file.
  The rendering style is fixed for the duration of the object, so to
  change the style you have to create a new object.

  To create maps for a tile, first set() the tile, render() it (once
  only), then call save() for each file that needs to be created.
  Note that you cannot save a file of higher resolution than maxLevel
  (given in the constructor).

  Rendering is done by creating an OpenGL framebuffer object and
  rendering to an attached texture of a size given by maxLevel
  (therefore your graphics card has to be able to support textures of
  that size).

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

#ifndef _TILEMAPPER_H_
#define _TILEMAPPER_H_

#include <vector>

#include <plib/pu.h>

#include "Palette.hxx"
#include "Tiles.hxx"
#include "Bucket.hxx"

class TileMapper {
  public:
    TileMapper(Palette *p,
	       unsigned int maxLevel = 10,
	       bool discreteContours = true,
	       bool contourLines = false,
	       float azimuth = 315.0,
	       float elevation = 55.0,
	       bool lighting = true,
	       bool smoothShading = true);
    ~TileMapper();

    enum ImageType {PNG, JPEG};

    // Specify the tile upon which future operations will operate.
    // This will load the scenery for the tile.
    void set(Tile *t);

    // Renders the current tile at the size given by maxLevel.
    // Rendering is done via a frame buffer object drawing into a
    // texture.  This only needs to be done once per tile.
    void render();

    // Save the current image to a file at the given level.  You must
    // call render() before the first call to save() (for each tile).
    // EYE - put in a check?
    void save(unsigned int level, ImageType t, unsigned int jpegQuality = 75);

    // Accessors.
    const Palette *palette() const { return _palette; }
    unsigned int maxLevel() const { return _maxLevel; }
    bool discreteContours() const { return _discreteContours; }
    float azimuth() const { return _azimuth; }
    float elevation() const { return _elevation; }
    bool contourLines() const { return _contourLines; }
    bool lighting() const { return _lighting; }
    bool smoothShading() const { return _smoothShading; }

  protected:
    void _unloadBuckets();

    // Our palette.
    Palette *_palette;
    // Maximum level (including any over-sampling).
    unsigned int _maxLevel;
    // True if we should have discrete contour colours.
    bool _discreteContours;
    // True if we should draw contour lines.
    bool _contourLines;
    // Position of light (in eye coordinates).
    float _azimuth, _elevation;
    sgVec4 _lightPosition;
    // True if we have lighting.
    bool _lighting;
    // True if we smooth polygons, false if we want "chunky" polygons.
    bool _smoothShading;

    // The tile we're working on.
    Tile *_tile;

    // Maximum elevation of tile in feet (this can only be set once
    // we've loaded the buckets).
    float _maximumElevation;

    // Our scenery.
    std::vector<Bucket *> _buckets;

    // The width and height of the full-sized tile.
    int _width, _height;

    // Framebuffer and texture objects.
    GLuint _fbo, _to;
};

#endif	// _TILEMAPPER_H_
