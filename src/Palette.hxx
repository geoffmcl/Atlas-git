/*-------------------------------------------------------------------------
  Palette.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2013 Brian Schack

  A palette tells Atlas (and Map) how to colour maps.  It specifies
  colours for elevations and materials.  The palette object can load
  an Atlas palette file.  After loaded, you can query the palette for
  the colour for a material or elevation.

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

#ifndef _PALETTE_H_
#define _PALETTE_H_

#include <string>
#include <map>
#include <deque>
#include <vector>

#include <plib/sg.h>		// sgVec4

class Palette {
  public:
    Palette(const char *path);
    ~Palette();

    const char *path() const { return _path.c_str(); }

    // This defines one contour interval.  All elevations at or above
    // 'elevation', up to but not including the next contour are given
    // the RGBA colour 'colour'.
    struct Contour {
	float elevation;
	sgVec4 colour;
    };

    // The base is an offset to elevation values.  By default it is
    // 0.0, which means that an elevation defined in the palette is
    // absolute.  By setting the base to a non-zero value, elevations
    // will be coloured relative to that point.
    float base() { return _base; }
    void setBase(float f);

    // Returns the contour struct for the given elevation.
    const Contour& contour(float elevation);

    // Returns the colour (RGBA) for the given material.  If
    // 'material' is not found, then NULL is returned.
    const float *colour(const char *material) const;

    // Calculates the smoothed colour for the given elevation, putting
    // the result in 'colour'.
    void smoothColour(float elevation, sgVec4 colour);

    // Returns the index of the contour struct for the given
    // elevation.
    unsigned int contourIndex(float elevation);
    // Returns the ith contour struct.  If i is too large, it returns
    // the last element.
    const Contour& contourAtIndex(unsigned int i) const;
    // Returns the number of valid contour intervals.  We subtract 2
    // because of the "fake" first and last Contour pairs.
    unsigned int size() const { return _elevations.size() - 2; }

  protected:
    // Our file.
    std::string _path;

    // Our units - true if metres, false if feet.
    bool _metres;

    // An offset to add to all elevations.
    float _base;

    // This is a hack to get around C/C++'s refusal to assign arrays.
    // By putting the array in a struct, it will do assignments for
    // us.  And no, 'foodle' doesn't mean anything.
    struct _foodle {
	sgVec4 c;
    };

    // Maps from a material name to a colour.
    std::map<std::string, _foodle> _materials;

    // Deque of <elevation, colour> pairs.  The first and last pairs
    // are generated internally by the Palette class to make
    // calculating smooth colour values easier.
    std::deque<Contour> _elevations;
    // This is a copy of _elevations, but with all elevations offset
    // by _base.
    std::vector<Contour> _offsetElevations;
    
    // Index of last colour index returned from contourIndex().  I'm
    // guessing that successive calls to this method will be for
    // similar elevations.  By starting the next search where the last
    // one ended, we're likely to be right the first time.  A few
    // tests have shown that this is true about 90% of the time.
    unsigned int _i;
};

#endif
