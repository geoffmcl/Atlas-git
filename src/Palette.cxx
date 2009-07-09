/*-------------------------------------------------------------------------
  Palette.cxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

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

#include "Palette.hxx"

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <fstream>

#include <simgear/constants.h>

using namespace std;

Palette::Palette(const char *path): _i(0)
{
    // EYE - throw errors?
    ifstream in(path);
    if (in.fail()) {
	throw std::runtime_error("failed to open file");
	return;
    }

    string buf;

    // Map from colour names to colours.
    map<string, _foodle> colourMap;
    string keyword, material, name;
    _foodle colour;
    bool metres = true;

    while (getline(in, buf)) {
	istringstream line(buf);
	if (line.str().size() == 0) {
	    continue;
	}

	if (line.str()[0] == '#') {
	    // Comment
	    continue;
	}

	line >> keyword;

	if (keyword == "Units") {
	    // Units <feet/ft/metres/meters/m>
	    line >> name;
	    if ((name == "feet") || 
		(name == "ft")) {
		metres = false;
	    } else if ((name == "metres") || 
		       (name == "meters") || 
		       (name == "m")) {
		metres = true;
	    } else {
		throw std::runtime_error("unknown unit");
	    }
	} else if ((keyword == "Colour") || (keyword == "colour")) {
	    // Colour <name> <R> <G> <B> <A>
	    line >> name;

	    for (int i = 0; i < 4; i++) {
		line >> colour.c[i];
	    }

	    if (colourMap.find(name) != colourMap.end()) {
		throw std::runtime_error("colour multiply defined");
	    } else {
		colourMap.insert(make_pair(name, colour));
	    }
	} else if (keyword == "Material") {
	    // Material <material> <name>
	    line >> material >> name;

	    if (colourMap.find(name) == colourMap.end()) {
		// Non-existent colour.
		continue;
	    }

	    int height;
	    if (sscanf(material.c_str(), "Elevation_%d", &height) == 1) {
		Contour entry;
		if (!metres) {
		    // Internally we use metres.
		    height *= SG_FEET_TO_METER;
		}
		entry.elevation = height;
		sgCopyVec4(entry.colour, colourMap[name].c);
		_elevations.push_back(entry);
	    } else if (_materials.find(material) == _materials.end()) {
		// EYE - need a list of all materials (to help users
		// create AtlasPalette files).

		// EYE - the earlier version would add Elevation_<x>
		// entries to the materials map as well.
		_materials.insert(make_pair(material, colourMap[name]));
	    } else {
		throw std::runtime_error("material multiply defined");
	    }
	} else {
	    fprintf(stderr, "%s: syntax error in line:\n\t'%s'\n", 
		    path, line.str().c_str());
	}
    }

    if (_elevations.size() == 0) {
	throw std::runtime_error("no elevations specified");
    }

    // Bubble sort the _elevations vector by height.
    bool unsorted = true;
    while (unsorted) {
	unsorted = false;
	// EYE - duplicate elevations?
	for (unsigned int i = 0; i < _elevations.size() - 1; i++) {
	    if (_elevations[i].elevation > _elevations[i + 1].elevation) {
		swap(_elevations[i], _elevations[i + 1]);
		unsorted = true;
	    }
	}
    }

    _path = path;
}

Palette::~Palette()
{
}

const Palette::Contour& Palette::contour(float elevation)
{
    return _elevations[contourIndex(elevation)];
}

const float *Palette::colour(const char *material) const
{
    map<string, _foodle>::const_iterator i = _materials.find(material);
    if (i != _materials.end()) {
	return i->second.c;
    }

    return NULL;
}

// Returns the contour index in the palette for the given elevation.
// An entry:
//
// Material Elevation_<x> <colour>
//
// means "everything at <x> or above is <colour>".
unsigned int Palette::contourIndex(float elevation)
{
    // Search up.
    for (; 
	 (_i < _elevations.size() - 1) && 
	     (elevation >= _elevations[_i + 1].elevation); 
	 _i++)
	;
    // Search down.
    for (; (_i > 0) && (elevation < _elevations[_i].elevation); _i--)
	;

    return _i;
}

const Palette::Contour& Palette::contourAtIndex(unsigned int i) const
{
    if (i < 0) {
	i = 0;
    } else if (i >= _elevations.size()) {
	i = _elevations.size() - 1;
    }

    return _elevations[i];
}

// Creates a smoothed colour value for the given elevation.  Assume we
// have following palette entries (where the colour index has been
// replaced by a variable for clarity):
//
// Material Elevation_0		c0
// Material Elevation_100	c1
// Material Elevation_200	c2
//
// c0 is used from 0 to 100, c1 from 100 to 200, and c2 above 200.
// Our goal is to use pure colours in the middle of their range, an
// equal mix at the boundaries, and a weighted blend elsewhere.
//
// Therefore:
//
// (1) an elevation of 100 gives a colour of c0 + c1 / 2.0.
//
// (2) an elevation of 125 gives a colour of c0 + 3 * c1 / 4.0
//
// (3) an elevation of 150 gives a colour of c1.
//
// (4) an elevation of 175 gives a colour of 3 * c1 + c2 / 4.0.
//
// (5) an elevation of 200 gives a colour of c1 + c2 / 2.0.
void Palette::smoothColour(float elevation, sgVec4 colour)
{
    unsigned int i = contourIndex(elevation);

    // These are used when smoothing colours.  Their standard meanings
    // are given below, although they have to be adjusted when at the
    // top or bottom contours.
    //
    // range - elevation range for the band the elevation is in
    // delta - difference between the elevation and the bottom of the
    //     band
    // scale - how much to scale the two colours
    float range, delta, scale;
    const float *cu, *cl;		// Upper and lower colours.

    if (i == (_elevations.size() - 1)) {
	// The top elevation slice is a special case.  First, it's
	// typically defined to extend way way past any possible
	// highest elevation.  This means that just linearly
	// interpolating will never give us a pure top colour.
	// Second, there is no higher colour to blend with.
	//
	// Ideally (in my opinion), a smooth map should look like an
	// unsmoothed map, just smoother.  Midpoints in a contour
	// interval should, in the smoothed map, have identical
	// colours to that in the unsmoothed map.  The colour along
	// the contour line should be equally composed of the upper
	// and lower colours.
	//
	// To solve this problem, we define the middle of the top
	// elevation slice (where the bottom colour's contribution is
	// 0) to be equal to the middle of the next lower slice.  So,
	// if the previous slice covers 1000m, and this slice starts
	// at 3000m, then the middle of this slice is 3500m.
	// Elevations below 3500m are a combination of this slice's
	// colour and the previous slice's colour. All elevations
	// above that will be coloured only with this slice's colour.
	range = (_elevations[i].elevation - _elevations[i - 1].elevation);
	delta = elevation - _elevations[i].elevation;
	scale = delta / range;
	scale += 0.5;
	if (scale > 1.0) {
	    scale = 1.0;
	}
	cu = _elevations[i].colour;
	cl = _elevations[i - 1].colour;
    } else if (i == 0) {
	// And the first level is a special case too - there's no
	// lower colour to blend with.  Everyting below the midway
	// point will be "pure" coloured.
	range = (_elevations[i + 1].elevation - _elevations[i].elevation);
	delta = elevation - _elevations[i].elevation;
	scale = delta / range;
	if (scale < 0.5) {
	    scale = 0.5;
	}
	scale -= 0.5;
	cu = _elevations[i + 1].colour;
	cl = _elevations[i].colour;
    } else {
	range = _elevations[i + 1].elevation - _elevations[i].elevation;
	delta = elevation - _elevations[i].elevation;
	scale = delta / range;
	if (scale > 0.5) {
	    // Blend with upper colour.  The lower colour varies from
	    // 1.0 (in the middle) to 0.5 (at the border), the upper
	    // colour from 0.0 (in the middle) to 0.5 (at the border).
	    scale = 1.5 - scale;
	    cu = _elevations[i].colour;
	    cl = _elevations[i + 1].colour;
	} else {
	    // Blend with lower colour.  The upper colour varies from
	    // 1.0 (in the middle) to 0.5 (at the border), the lower
	    // colour from 0.0 (in the middle) to 0.5 (at the border).
	    scale += 0.5;
	    cu = _elevations[i].colour;
	    cl = _elevations[i - 1].colour;
	}
    }

    sgSubVec4(colour, cu, cl);
    sgScaleVec4(colour, scale);
    sgAddVec4(colour, cl);
}
