/*-------------------------------------------------------------------------
  TileMapper.hxx

  Written by Brian Schack, started March 2009.

  Copyright (C) 2009 - 2012 Brian Schack

  This class will render maps for a tile and write them to a file.
  The rendering style is fixed for the duration of the object, so to
  change the style you have to create a new object.  This may seem a
  bit of an odd approach, but the motiviation is that we tend to
  render all maps in the same way; all that changes are the tiles
  being mapped.  A TileMapper can be thought of as an encapsulation of
  a certain rendering style, which is then applied to various tiles.

  To create maps for a tile, first set() the tile, render() it (once
  only), then call save() for each file that needs to be created.
  Note that you cannot save a file of higher resolution than
  maxDesiredLevel (given in the constructor).

  Rendering is done by creating an OpenGL framebuffer object and
  rendering to an attached texture of a size given by maxDesiredLevel
  (therefore your graphics card has to be able to support textures of
  that size - this can be checked with the maxPossibleLevel() class
  method).

  There must be an valid OpenGL context when a TileMapper object is
  created and used.

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

#include <plib/pu.h>		// sgVec4

// Forward class declarations
class Palette;
class Tile;
class Bucket;

class TileMapper {
  public:
    // Returns the maximum map level that this computer's graphics
    // card can theoretically handle.  A map level of n means that the
    // card can handle textures and frame buffers of 2^n x 2^n.  The
    // result should be treated with a grain of salt as it is just an
    // estimate and can't take into account texture memory being used
    // by other textures - in reality, the you might have to decrease
    // the maximum level by 1.
    static unsigned int maxPossibleLevel();

    enum ImageType {PNG, JPEG};

    // Create a tile mapper with the given rendering parameters.
    TileMapper(Palette *p,
    	       unsigned int maxDesiredLevel = 10,
    	       bool discreteContours = true,
    	       bool contourLines = false,
    	       float azimuth = 315.0,
    	       float elevation = 55.0,
    	       bool lighting = true,
    	       bool smoothShading = true,
	       ImageType imageType = JPEG,
	       unsigned int jpegQuality = 75);
    ~TileMapper();

    // Specify the tile upon which future operations will operate.
    // This will load the scenery for the tile.  If t is NULL, this
    // essentially clears the current values (and calls to render() or
    // save() are ignored).
    void set(Tile *t);

    // Renders the current tile at the size given by maxLevel.
    // Rendering is done via a frame buffer object drawing into a
    // texture.  This only needs to be done once per tile.
    void render();

    // Save the current image to a file at the given level (<=
    // maxDesiredLevel).  You must call render() before the first call
    // to save() (for each tile).
    void save(unsigned int level);

    // Accessors.
    const Palette *palette() const { return _palette; }
    unsigned int maxDesiredLevel() const { return _maxLevel; }
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
    // JPEG or PNG
    ImageType _imageType;
    // If JPEG, this gives our image quality.
    unsigned int _JPEGQuality;

    // The tile we're working on.
    Tile *_tile;

    // Maximum elevation of tile in feet (this can only be set once
    // we've loaded the buckets).
    float _maximumElevation;

    // Our scenery.
    std::vector<Bucket *> _buckets;

    // The width and height of the full-sized tile.
    int _width, _height;

    // Our texture object, the ultimate destination for our rendering.
    GLuint _to;
};

#endif	// _TILEMAPPER_H_
