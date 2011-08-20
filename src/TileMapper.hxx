/*-------------------------------------------------------------------------
  TileMapper.hxx

  Written by Brian Schack, started March 2009.

  Copyright (C) 2009 - 2011 Brian Schack

  This class will render maps for a tile and write them to a file.
  The rendering style is fixed for the duration of the object, so to
  change the style you have to create a new object.

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

#include <plib/pu.h>

#include "Palette.hxx"
#include "Tiles.hxx"
#include "Bucket.hxx"

class TileMapper {
  public:
    TileMapper(Palette *p, 
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
    void set(TileInfo *t);

    // Draw the current tile at the given level.  This can be called
    // multiple times quite efficiently, as the OpenGL commands are
    // saved in a display list.
    void draw(unsigned int level);
    // Save the current image to a file at the given level.  This
    // level need not be the same as the level at which it was
    // rendered (this can be used to do oversampling).
    void save(unsigned int level, ImageType t, unsigned int jpegQuality = 75);

  protected:
    void _unloadBuckets();

    // Our palette.
    Palette *_palette;
    // True if we should have discrete contour colours.
    bool _discreteContours;
    // True if we should draw contour lines.
    bool _contourLines;
    // Position of light (in eye coordinates).
    sgVec4 _lightPosition;
    // True if we have lighting.
    bool _lighting;
    // True if we smooth polygons, false if we want "chunky" polygons.
    bool _smoothShading;

    // The tile we're working on.
    TileInfo *_tile;

    // Maximum elevation of tile in feet (this can only be set once
    // we've loaded the buckets).
    float _maximumElevation;

    // Our scenery.
    std::vector<Bucket *> _buckets;
};

#endif	// _TILEMAPPER_H_
